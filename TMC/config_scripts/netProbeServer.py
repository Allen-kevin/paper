import sys
import os
import socket
import threading
import time
from time import gmtime, strftime

QUIT = False
bufSize = 1024

host = '192.168.50.50'
listen_port = int(sys.argv[1])
server_addr = (host, listen_port)

class ClientThread( threading.Thread ):
    def __init__( self, client_sock ):
        threading.Thread.__init__( self )
        self.client = client_sock

    def run( self ):
      try:
	
        cmd = self.client.recv(bufSize)
        print cmd.strip().split()[0], cmd.strip().split()[1], cmd.strip().split()[2]       
        if 'download' == cmd.strip().split()[0]:
	   startTime = time.time()
	   bytes = 0
	   while time.time() - startTime <= float(cmd.strip().split()[2]):
		bytes += self.client.send(' ' * bufSize)
		timeUsed = time.time() - startTime
		if float(cmd.strip().split()[1]) > 0:
			timeInterval = float(bytes) / (float(cmd.strip().split()[1])*float(1024*1024)/float(8))
		
		if float(cmd.strip().split()[1]) > 0 and timeInterval - timeUsed > 0 :
			time.sleep(float(timeInterval) - float(timeUsed))
		
	   self.client.send('Close')
	   throughput = float(bytes)*8/float(time.time() - startTime)
	   result = self.client.recv(bufSize)
	   print result.strip()
	   print throughput
        elif 'upload' == cmd.strip().split()[0]:
		totalByte = 0
		startTime = time.time()
		done = False
		
		while not done:
			data = self.client.recv(bufSize)
			totalByte += len(data)
			if data: 
				if data.strip() == 'Close':
					done = True
					#print data.strip()
				else:
					throughput = float(totalByte)/float(time.time() - startTime)
					#print throughput
		print self.client.getsockname(), str(throughput*8) 
		#self.client.send(str(throughput*8))
		
        self.client.close()
      except KeyboardInterrupt:
        self.client.close()
        print 'Catch Ctrl+C pressed... Shutting Down'
	return
      except Exception, err:
        self.client.close()
        print 'Catch Exception : %s\nClosing...' % err
	return

class Server:
   
    def __init__( self ):
        self.sock = None
        self.thread_list = []

    def run( self ):

        all_good = False
        try_count = 0

        while not all_good:
            if 3 < try_count:
                
                sys.exit( 1 )
            try:
               
                self.sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
		self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)

                self.sock.bind( server_addr )
               
                self.sock.listen( 5 )
                all_good = True
                break
            except socket.error, err:
                
                print 'Socket connection error... Waiting 10 seconds to retry.'
                del self.sock
                time.sleep( 10 )
                try_count += 1

        print "Server is listening for incoming connections."

        try:
            while not QUIT:
                try:
              
                    self.sock.settimeout( 0.500 )
                    client = self.sock.accept()[0]
                except socket.timeout:
                   
                    time.sleep( 1 )
                    if QUIT:
                        print "Received quit command. Shutting down..."
                        break
                    continue
               
                new_thread = ClientThread( client )
                print 'Incoming Connection. Started thread ',
                print new_thread.getName()
                self.thread_list.append( new_thread )
                new_thread.start()

                for thread in self.thread_list:
                    if not thread.isAlive():
                        self.thread_list.remove( thread )
                        thread.join()

        except KeyboardInterrupt:
            print 'Ctrl+C pressed... Shutting Down'
        except Exception, err:
            print 'Exception caught: %s\nClosing...' % err

        for thread in self.thread_list:
            thread.join( 1.0 )
        
        self.sock.close()

if "__main__" == __name__:
    server = Server()
    server.run()

    print "Terminated"
