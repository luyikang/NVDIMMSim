import sys

# capacity parameters
NUM_PACKAGES = 32
DIES_PER_PACKAGE = 4
PLANES_PER_DIE = 1
BLOCKS_PER_PLANE = 32
PAGES_PER_BLOCK = 48

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

# derived parameter

TOTAL_PLANES = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE
CONCURRENCY = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE
CAPACITY = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE * PAGES_PER_BLOCK

READ_TIME = (READ_CYCLES + COMMAND_LENGTH) * CYCLE_TIME
WRITE_TIME = (WRITE_CYCLES + COMMAND_LENGTH) * CYCLE_TIME
ERASE_TIME = (ERASE_CYCLES + COMMAND_LENGTH) * CYCLE_TIME

# with the perfect scheduling version of plane state we do need to keep track of the planes
planes = [[[0 for i in range(PLANES_PER_DIE)] for j in range(DIES_PER_PACKAGE)] for k in range(NUM_PACKAGES)]

# list of pending writes that this analysis creates
pending_writes = []

# get the log files
write_log = open(sys.argv[1], 'r')
plane_log = open(sys.argv[2], 'r')
# are we just getting read statistics or are we trying to schedule writes
mode = sys.argv[3]

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
	#read in the plane states log
	#find the state of the planes up to this time
	while(1):
		state = plane_log.readline()
		# if the state is blank we've reached the end of the file
		if state == '':
			break

		if state == 'Plane State Log \n':
			#do nothing for now
			print 'starting plane state parsing'
		else:
			[state_cycle, state_address, package, die, plane, op] = [int(j) for j in state.strip().split()]
			if op == 0:
				idle[package][die][plane] = 1
				last_read[package][die][plane] = state_cycle

			if idle[package][die][plane] == 1 and op != 0:
				temp_time = state_cycle - last_read[package][die][plane]
				if temp_time < shortest_time:
					shortest_time = temp_time
				elif temp_time > longest_time:
					longest_time = temp_time

				if temp_time > WRITE_CYCLES:
					open_count = open_count + 1
					open_counts[package][die][plane] = open_counts[package][die][plane] + 1
				
				average_times[package][die][plane] = average_times[package][die][plane] + temp_time
				idle_counts[package][die][plane] = idle_counts[package][die][plane] + 1
			
			
	for i in range(NUM_PACKAGES):
		for j in range(DIES_PER_PACKAGE):
			for k in range(PLANES_PER_DIE):	
				if idle_counts[i][j][k] > 0:			
					average_times[i][j][k] = average_times[i][j][k] / idle_counts[i][j][k]
					average_time = average_time + average_times[i][j][k]

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
	cycle = 0
	delayed_writes = 0
	delayed_reads = 0
	completed_writes = 0
	free_planes = TOTAL_PLANES
	delayed = 0 
	while(1):
		if delayed == 0:
			# get the next write
			write = write_log.readline()
			# if the write is blank we've reached the end of the file
			if write == '':
				break
			# if the write is the title of the file then we're at the beginning
			if write == 'Write Arrival Log \n':
				# do nothing for now
				print 'starting write arrival parsing'
			else:
				# parse the write data
				[tcycle, address] = [int(i) for i in write.strip().split()]
				if tcycle > cycle:
					cycle = tcycle
				else:
					delayed_writes = delayed_writes + 1		
		else:
			# increment the cycle count
			cycle = cycle + 1
	
		#find the state of the planes up to this time
		while(1):
			state = plane_log.readline()
			# if the state is blank we've reached the end of the file
			if state == '':
				break
	
			if state == 'Plane State Log \n':
				#do nothing for now
				print 'starting plane state parsing'
			else:
				[state_cycle, state_address, package, die, plane, op] = [int(j) for j in state.strip().split()]
	
				# if the cycle of this state change is greater than the write arrival 	cycle
				# break cause we're not here yet
				if state_cycle > cycle:
					break
	
				# if the plane is newly idle update the planes count to reflect a new 	open
				# plane
				if planes[package][die][plane] != 0 and op == 0:
					free_planes = free_planes + 1
				elif planes[package][die][plane] == 0 and op != 0:
					free_planes = free_planes - 1				
				
				# if this read needed a plane that we didn't have then it would have 
				# been delayed so record that
				if free_planes < 0:
					delayed_reads = delayed_reads + 1

				planes[package][die][plane] = op
				
					
			
		#check to see if any pending writes are done
		for p in pending_writes:
			if p <= cycle:
				free_planes = free_planes + 1
				pending_writes.remove(p)
				completed_writes = completed_writes + 1
	
		#is there a free plane for this write
		if free_planes == 0 and delayed == 0:
			delayed_writes = delayed_writes + 1
			delayed = 1
		#issue the write to a plane and determine its completion cycle
		elif free_planes > 0:
			free_planes = free_planes - 1
			pending_writes.append(cycle + WRITE_CYCLES)
			delayed = 0
			#print 'free planes', free_planes
			#print 'number of pending writes', len(pending_writes)
 		 

		
		# still need to finish this up by also adding checks for channel contention, I think



	print 'free planes', free_planes
	print 'delayed reads', delayed_reads
	print 'delayed writes', delayed_writes	
	print 'completed writes', completed_writes

else:
	print 'invalid mode selection, please enter either Read or Write'
	
		
		
