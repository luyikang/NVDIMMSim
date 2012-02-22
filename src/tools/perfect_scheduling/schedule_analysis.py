import sys
import math

# capacity parameters
NUM_PACKAGES = 32
DIES_PER_PACKAGE = 4
PLANES_PER_DIE = 1
BLOCKS_PER_PLANE = 32
PAGES_PER_BLOCK = 48
NV_PAGE_SIZE=32768 # in bits

# timing parameters
DEVICE_CYCLE = 2.5
CHANNEL_CYCLE = 0.15
DEVICE_WIDTH = 8
CHANNEL_WIDTH = 8

READ_CYCLES = 16678 # pretty sure I don't need this actually
WRITE_CYCLES = 133420
ERASE_CYCLES = 1000700
COMMAND_LENGTH = 56

CYCLE_TIME = 1.51

# derived parameters

TOTAL_PLANES = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE
CONCURRENCY = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE
CAPACITY = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK

CYCLES_PER_TRANSFER = NV_PAGE_SIZE / CHANNEL_WIDTH

READ_TIME = (READ_CYCLES + COMMAND_LENGTH) * CYCLE_TIME
WRITE_TIME = (WRITE_CYCLES + COMMAND_LENGTH) * CYCLE_TIME
ERASE_TIME = (ERASE_CYCLES + COMMAND_LENGTH) * CYCLE_TIME

# get the log files
write_log = open(sys.argv[1], 'r')
plane_log = open(sys.argv[2], 'r')
# are we just getting read statistics or are we trying to schedule writes
mode = sys.argv[3]

# preparse everything into files
plane_data = []
write_data = []

# analyzing reads only to find out the space between them
if mode == 'Read':
	# idle time records
	shortest_time = READ_TIME * WRITE_TIME #just a big number
	longest_time = 0
	average_time = 0
	average_times = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	# potential write records
	open_count = 0
	open_counts = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	last_read = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	idle = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	idle_counts = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]
	data_counter = 0

	# get all the plane log data
	while(1):
		state = plane_log.readline()
		# if the state is blank we've reached the end of the file
		if state == '':
			break
	
		if state == 'Plane State Log \n':
			#do nothing for now
			print 'starting plane state parsing'
			continue
	
		plane_data.append(state)

	#read in the plane states log
	#find the state of the planes up to this time
	while(1):
		# check to see if we're done
		if data_counter >= len(plane_data):
			break

		curr_data = plane_data[data_counter]
		[state_cycle, state_address, package, die, plane, op] = [int(j) for j in curr_data.strip().split()]
		# if this is a record of a plane going idle record that plane as now idle and this cycle as the end
		# of its last read
		if op == 0:
			idle[package][die][plane] = 1
			last_read[package][die][plane] = state_cycle
		
		# if this is a record of a plane starting a read and that plane was idle then compute the amount of time
		# this plane was idle
		if idle[package][die][plane] == 1 and op != 0:
			temp_time = state_cycle - last_read[package][die][plane]
			if temp_time < shortest_time:
				shortest_time = temp_time
			elif temp_time > longest_time:
				longest_time = temp_time

			# was this plane idle long enough that it could have done a write?
			if temp_time > WRITE_CYCLES:
				open_count = open_count + math.floor(temp_time/WRITE_CYCLES)
				open_counts[package][die][plane] = open_counts[package][die][plane] + 1
				
			average_times[package][die][plane] = average_times[package][die][plane] + temp_time
			idle_counts[package][die][plane] = idle_counts[package][die][plane] + 1
		# done with that line of the log, move on to the next one
		data_counter =  data_counter + 1
			
	# add everything up for the total average across all planes		
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):	
				if idle_counts[i][j][k] > 0:			
					average_times[i][j][k] = average_times[i][j][k] / idle_counts[i][j][k]
					average_time = average_time + average_times[i][j][k]

	# divide to get the average
	average_time = average_time / TOTAL_PLANES
				
	print 'shortest idle time', shortest_time
	print 'longest idle time', longest_time
	print 'average idle time', average_time
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):
				print 'average time for package', i, 'die', j, 'plane', k, 'is', average_times[i][j][k]
	print 'number of idle times large enough for a write', open_count
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):
				print 'write sized gaps for package', i, 'die', j, 'plane', k, 'is', open_counts[i][j][k]

# trying to place writes between the reads and determining if our actions delay either
elif mode == 'Write':

	# with the perfect scheduling version of plane state we do need to keep track of the planes
	planes = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]

	# list of pending writes that this analysis creates
	pending_writes = []

	# list of times when channels will be free
	busy_channels = []

	# list of times when channels will be used
	soon_channels = []

	# list of the addresses of pending writes to make sure that a read doesn't go through before its corresponding write
	pending_addresses = {} #dictionary cycle:address

	cycle = 0
	delayed_writes = 0
	delayed_reads = 0
	completed_writes = 0
	completed_reads = 0
	free_planes = TOTAL_PLANES
	free_channels = NUM_PACKAGES
	write_delayed = 0
	read_delayed = 0
	channel_delays = 0
	RAW_haz = 0 

	#reading counters
	write_counter = 0
	plane_counter = 0
	
	# get all the write log data
	while(1):
		write = write_log.readline()

		if write == '':
			break

		elif write == 'Write Arrival Log \n':
			#do nothing for now
			print 'starting write arrival parsing'
			continue
	
		write_data.append(write)
	
	# get all the plane log data
	while(1):
		state = plane_log.readline()
		# if the state is blank we've reached the end of the file
		if state == '':
			break
	
		if state == 'Plane State Log \n':
			#do nothing for now
			print 'starting plane state parsing'
			continue
	
		plane_data.append(state)
	
	while(1):
		if write_delayed == 0:
			# see if we're done
			if write_counter >= len(write_data):
				break

			# parse the write data
			curr_write = write_data[write_counter]
			[tcycle, address] = [int(i) for i in curr_write.strip().split()]
			if tcycle > cycle:
				cycle = tcycle
			else:
				delayed_writes = delayed_writes + 1	

			# we can move on
			write_counter = write_counter + 1	
		else:
			# increment the cycle count
			cycle = cycle + 1
	
		#find the state of the planes up to this time
		while(1):
			# see if we're done here too
			if plane_counter >= len(plane_data):
				break
			
			# parse the read data
			curr_op = plane_data[plane_counter]
			[state_cycle, state_address, package, die, plane, op] = [int(j) for j in curr_op.strip().split()]
	
			# if the cycle of this state change is greater than the write arrival cycle
			# break cause we're not here yet
			if state_cycle > cycle:
				break

			# if the cycle of this state change is the same or greater than the cycle of when a
			# read was to start transfering, start the transfer
			for t in soon_channels:
				if t >= cycle:
					free_channels = free_channels - 1
					soon_channels.remove(t)
		
					# if this read needed a channel to transfer its data and we didn't have it then
					# it would have been delayed so record that
					if free_channels < 0:
						delayed_reads = delayed_reads + 1
						channel_delays = channel_delays + 1
										

			# if the plane is newly idle update the planes count to reflect a new open
			# plane
			if planes[package][die][plane] != 0 and op == 0:
				free_planes = free_planes + 1
				# channel is now no longer being used
				free_channels = free_channels + 1
				completed_reads = completed_reads + 1
			elif planes[package][die][plane] == 0 and op != 0:
				free_planes = free_planes - 1	
				# let us know that a channel will go busy when this read is done
				soon_channels.append(cycle + READ_CYCLES)			
				
				# if this read needed a plane that we didn't have then it would have 
				# been delayed so record that
				if free_planes < 0:
					delayed_reads = delayed_reads + 1
	
				# if this read was for a write that hasn't yet finished, record the error
				if state_address in pending_addresses:
					delayed_reads = delayed_reads + 1
					RAW_haz = RAW_haz + 1

			planes[package][die][plane] = op

			# got to the end so we're good to move on the next read record
			plane_counter = plane_counter + 1					
			
		#check to see if any pending writes are done
		for p in pending_writes:
			if p <= cycle:
				free_planes = free_planes + 1
				pending_writes.remove(p)
				del pending_addresses[p]
				completed_writes = completed_writes + 1

		#check to see if we're done using a channel to transfer the data
		for c in busy_channels:
			if c <= cycle:
				free_channels = free_channels + 1
				busy_channels.remove(c)
	
		#is there a free plane and channel for this write
		if free_planes == 0 or free_channels == 0 and write_delayed == 0:
			delayed_writes = delayed_writes + 1
			write_delayed = 1
		#issue the write to a plane and determine its completion cycle
		elif free_planes > 0 and free_channels > 0:
			free_planes = free_planes - 1
			pending_writes.append(cycle + WRITE_CYCLES + CYCLES_PER_TRANSFER)
			pending_addresses[cycle + WRITE_CYCLES + CYCLES_PER_TRANSFER] = address
			delayed = 0
			# gonna be using the channel immediately
			free_channels = free_channels - 1
			busy_channels.append(cycle + CYCLES_PER_TRANSFER)
 		 
	print 'free planes', free_planes
	print 'free channels', free_channels
	print 'delayed reads', delayed_reads
	print 'delayed writes', delayed_writes
	print 'channel delays', channel_delays	
	print 'completed writes', completed_writes
	print 'completed reads', completed_reads
	print 'RAW hazards', RAW_haz

else:
	print 'invalid mode selection, please enter either Read or Write'
	
		
		
