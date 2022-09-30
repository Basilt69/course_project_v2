require 'socket'

starttime = Process.clock_gettime(Process::CLOCK_MONOTONIC)
s = TCPSocket.new 'localhost',8989

s.write("/home/basiltkachenko/CLionProjects/course_project_2022/tmp/testfiles/1.c\n")
#s.write("/home/basiltkachenko/CLionProjects/course_project_2022/tmp/testfiles/#{ARGV[0]}.c\n")

s.each_line do |line|
    puts line
end

s.close
endtime = Process.clock_gettime(Process::CLOCK_MONOTONIC)
elapsed = endtime - starttime
puts "Elapsed: #{elapsed}"
#puts "Elapsed: #{elapsed} (#{ARGV[0]})"