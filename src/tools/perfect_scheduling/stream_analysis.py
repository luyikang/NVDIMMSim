#*********************************************************************************
#  Copyright (c) 2011-2012, Paul Tschirhart
#                             Jim Stevens
#                             Peter Enns
#                             Ishwar Bhati
#                             Mu-Tien Chang
#                             Bruce Jacob
#                             University of Maryland 
#                             pkt3c [at] umd [dot] edu
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  
#     * Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#  
#     * Redistributions in binary form must reproduce the above copyright notice,
#        this list of conditions and the following disclaimer in the documentation
#        and/or other materials provided with the distribution.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#********************************************************************************

import sys
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# capacity parameters
NUM_PACKAGES = 32
DIES_PER_PACKAGE = 4
PLANES_PER_DIE = 1
BLOCKS_PER_PLANE = 32
PAGES_PER_BLOCK = 48
NV_PAGE_SIZE=32768 # in bits

file_out = 0
image_out = "StreamAnalysis.pdf"
epoch_size = 100000

# get the log files
write_log = open(sys.argv[1], 'r')
plane_log = open(sys.argv[2], 'r')

if len(sys.argv) == 6:
	image_out = sys.argv[3]
	epoch_size = sys.argv[4]
	file_out = 1 # just so we know later to write to a file
	output_file = open(sys.argv[5], 'w')
elif len(sys.argv) == 5:
	image_out = sys.argv[3]
	epoch_size = int(sys.argv[4])
elif len(sys.argv) == 4:
	image_out = sys.argv[3]
	

# preparse everything into files
plane_data = []
write_data = []

# just so we can initialize the epochs arrays we need to know how many epochs to expect
plane_last_clock = 0
write_last_clock = 0

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
	[state_cycle, state_address, package, die, plane, op] = [int(j) for j in state.strip().split()]
	plane_last_clock = state_cycle
	
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
	[tcycle, address] = [int(i) for i in write.strip().split()]
	write_last_clock = tcycle

epoch = 0
last_clock = write_last_clock if write_last_clock > plane_last_clock else plane_last_clock
epoch_total = int(last_clock / epoch_size) + 1
epoch_writes = [0 for k in range(epoch_total)]
epoch_reads = [0 for k in range(epoch_total)]

write_pointer = 0
read_pointer = 0
writes_done = 0
reads_done = 0

while(1):
	#count the writes in this epoch
	while(1):
		# check to see if we're done
		if write_pointer >= len(write_data):
			writes_done = 1
			break

		curr_write = write_data[write_pointer]
		[tcycle, address] = [int(i) for i in curr_write.strip().split()]
		if tcycle < ((epoch + 1) * epoch_size):
			epoch_writes[epoch] = epoch_writes[epoch] + 1
		else:
			if file_out == 1:
				s =  str(epoch)
				output_file.write(s)
				output_file.write(" ")
				s =  str(epoch_writes[epoch])
				output_file.write(s)
				output_file.write(" ")

			break

		write_pointer = write_pointer + 1;

	#count the reads in this epoch
	while(1):
		# check to see if we're done
		if read_pointer >= len(plane_data):
			reads_done = 1
			break

		curr_read = plane_data[read_pointer]
		[state_cycle, state_address, package, die, plane, op] = [int(j) for j in curr_read.strip().split()]
		if state_cycle < ((epoch + 1) * epoch_size):
			if op != 0:
				epoch_reads[epoch] = epoch_reads[epoch] + 1
		else:
			if file_out == 1:
				s =  str(epoch_reads[epoch])
				output_file.write(s)
				output_file.write("\n")
			break

		read_pointer = read_pointer + 1;
	
	if writes_done == 1 and reads_done == 1:
		break
	
	epoch = epoch + 1

#Now we have two full arrays to graph so lets graph them
epoch_list = range(epoch_total)
plt.plot(epoch_list, epoch_reads, label = "Reads")
plt.plot(epoch_list, epoch_writes, label = "Writes")
plt.legend()
plt.show()
plt.savefig(image_out)
