/*
 *
 * NetConnection: Abstraction for a network connection.
 * $Id$
 *
 * Written by:
 *   Author: Eric Luke
 *   Department of Computer Science
 *   University of Utah
 *   Date: April 2001
 *
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/times.h>
#include <Network/NetConnection.h>
#include <Network/NetDispatchManager.h>

#include <Properties/ClientProperties.h>
#include <Malloc/Allocator.h>
#include <Logging/Log.h>

namespace SemotusVisum {
namespace Network {

#define _USE_DETACH
#define CLK_TCK 100 // TEMPORARY CODE for compilation purposes -- remove me!

using namespace SemotusVisum::Logging;
//using namespace SemotusVisum::Thread;
using namespace SCIRun;
using namespace SemotusVisum::Properties;

NetConnectionReader::NetConnectionReader( NetConnection *parent ) :
  dieNow( false ), done( false ), parent( parent ) {
  cerr << "In NetConnectionReader::NetConnectionReader, thread id is " << pthread_self() << endl;
  cerr << "End of NetConnectionReader::NetConnectionReader, thread id is " << pthread_self() << endl;
}

NetConnectionReader::~NetConnectionReader() {
}

void
NetConnectionReader::run() {
  cerr << "In NetConnectionReader::run, thread id is " << pthread_self() << endl;

  /* Wait for data to be read on our connection. */
  std::cerr << "NCR is pid " << getpid() << endl;
  parent->readerPID = getpid();

  //SemotusVisum::Thread::Thread::self()->makeInterruptable();
  while ( !dieNow ) {
    
    unsigned dataSize;
    int bytesread = 0;
    char buffe[256];
    dataSize = 0;

    Log::log( Logging::DEBUG, buffe );
    /* Read the data size from the network. */
    if ( ( bytesread =
	   ( parent->connection.read( (char *)&dataSize,
				      sizeof(unsigned int) ) ) )
	 < (int)sizeof(unsigned int) ) {

      // If we've timed out, continue
      if ( bytesread == -9 )
	continue;
      
      // If we've already dealt with this, just return.
      if ( parent->cleanDisconnect ) {
	done = true;
	Log::log( Logging::DEBUG, "Reader exiting1" );
	//return;
	Thread::exit();
      }
    
      sprintf( buffe, "Only read %d of %d bytes!", bytesread,
	       sizeof( unsigned int ) );
      Log::log( Logging::ERROR,
		buffe );

      /* Disconnection code. */
      char *buf = scinew char[ strlen( parent->name ) +
			  strlen( strerror( errno ) ) +
			  256 ];
      
      sprintf( buf, "Client %s disconnected uncleanly. Reason: %s",
	       parent->name, strerror(errno) );
      Log::log( Logging::WARNING, buf );
      delete buf;
      
      parent->cleanDisconnect = false;
      parent->netMonitor->getMailbox().send( parent );
      done = true;
      Log::log( Logging::DEBUG, "Reader exiting2" );
      return;
    }

    /* Convert the data */
    DataDescriptor dd = SIMPLE_DATA( UNSIGNED_INT_TYPE, 1 );
    ConvertNetworkToHost( (void *)&dataSize,
			  (void *)&dataSize,
			  &dd,
			  1 );

    //std::cerr << "\tAfter: " << dataSize << " bytes" << endl;
    sprintf( buffe, "Upcoming data is %u bytes!", dataSize );
    Log::log( Logging::DEBUG, buffe );

    if ( dataSize == 0 )
      continue; // EJL
    
    /* The client always sends us text */
    char * buffer = scinew char[ dataSize + 1 ];
    memset( buffer, 0, dataSize + 1 );
    
    if ( dieNow ) {
      done = true;
      Log::log( Logging::DEBUG, "Reader exiting3" );
      return;
    }
    
    if ( buffer == NULL )
      continue;
    int bytesRead = parent->connection.read( buffer, dataSize );
    snprintf( buffe, 256, "Done reading: %s", parent->name );
    Log::log( Logging::DEBUG, buffe );
    

    
    // If we got an error, and weren't interrupted, we're probably
    // no longer connected...
    if ( bytesRead <= 0 ) {
      delete buffer;
      
      char *buf = scinew char[ strlen( parent->name ) +
			  strlen( strerror( errno ) ) +
			  256 ];
      sprintf( buf, "Client %s disconnected. Reason: %s",
	       parent->name, strerror(errno) );
      Log::log( Logging::WARNING, buf );
      delete buf;
      
      
      if ( dieNow ) {
	done = true;
	Log::log( Logging::DEBUG, "Reader exiting4" );
	return;
      }
      
      parent->netMonitor->getMailbox().send( parent );
      done = true;
      Log::log( Logging::DEBUG, "Reader exiting5" );
      return;
    }
    char buf[100];
    
    sprintf( buf, "Read %d bytes", bytesRead );
    Log::log( Logging::DEBUG, buf );
    Log::log( Logging::DEBUG, buffer );

    /* Convert the data - the client always sends us text */
    char * buffer1 = scinew char[ dataSize + 1 ];
    HomogenousConvertNetworkToHost( (void *)buffer1, (void *)buffer,
				    CHAR_TYPE, dataSize + 1 );

    // Pass the data to the network dispatch manager.
    if ( dieNow ) {
      done = true;
      Log::log( Logging::DEBUG, "Reader exiting6" );
      return;
    }
    if ( NetConnection::useDispatchManager )
      NetDispatchManager::getInstance().fireCallback( buffer1,
						      bytesRead,
						      parent->name );
    else {
      /* Call the callback function if it isn't NULL */
      if ( NetConnection::callbackFunction != NULL ) {
	(*NetConnection::callbackFunction)( (void *)buffer1 );
      }
    }
    delete buffer;
    delete buffer1;
    Log::log( Logging::DEBUG, "Restarting read loop" );
  }
  Log::log( Logging::DEBUG, "Reader exiting7" );
  cerr << "End of NetConnectionReader::run, thread id is " << pthread_self() << endl;
}

NetConnectionWriter::NetConnectionWriter( NetConnection *parent ) :
  done( false ), dieNow( false ), parent( parent ) {
  cerr << "In NetConnectionWriter::NetConnectionWriter, thread id is " << pthread_self() << endl;
  cerr << "End of NetConnectionWriter::NetConnectionWriter, thread id is " << pthread_self() << endl;
}

NetConnectionWriter::~NetConnectionWriter() {
}

void
NetConnectionWriter::run() {
  cerr << "In NetConnectionWriter::run, thread id is " << pthread_self() << endl;
  dataItem d;
  struct tms dummy;
  double start, end;
  double ci = 1./CLK_TCK;
  
  /* Wait for data to be written on our connection. */
  std::cerr << "NCW is pid " << getpid() << endl;

  //SemotusVisum::Thread::Thread::self()->makeInterruptable();
  while ( !dieNow ) {
  
    /* Writing data - wait for messages (data) in our inbox */
    d = parent->mbox.receive();

    start = (double)times(&dummy)*ci;
    
    if ( dieNow ) {
      Log::log( Logging::DEBUG, "Writer exiting" );
      done = true;
      return;
    }
    
    int bytesWritten = 0;
    unsigned int dataSize = d.getSize();
    cerr << "NetConnectionWriter::run - dataSize = " << dataSize << endl;

    Log::log( Logging::DEBUG, "Writing message to network:" );
    Log::log( Logging::DEBUG, d.getData() );
    
    /* Convert the data size */
    DataDescriptor dd = SIMPLE_DATA( UNSIGNED_INT_TYPE, 1 );
    ConvertHostToNetwork( (void *)&dataSize,
			  (void *)&dataSize,
			  &dd,
			  1 );
			  
    // Write the data size to our connection
    bytesWritten = parent->connection.write( (const char *)&dataSize,
					     sizeof( unsigned int ) );
    std::cerr << "Wrote " << bytesWritten << " bytes." << endl;

    
    if ( bytesWritten != sizeof( unsigned int ) ) {
      char buffer[1000];
      
      snprintf( buffer, 100,
		"Write error - wrote %d bytes (data size) on connection %s",
		bytesWritten, parent->name );
      Log::log( Logging::ERROR, buffer );

      // Punt.
      parent->cleanDisconnect = false;
      parent->netMonitor->getMailbox().send( parent );
      done = true;
      Log::log( Logging::DEBUG, "Writer exiting" );
      return;
    }
    
   
    //std::cerr << "Writing data to client" << endl;

    /* If the type of the data is -1, we assume that the data has been
       blessed - ie, it is already suitable for network consumption.
       Only if the data type is not == -1 do we convert the data. */
    if ( d.getType() != -1 /*Mandatory*/&& d.getType() != -2 ) {
      

      /*      if ( d.getType() == CHAR_TYPE ) {
	//std::cerr << "Data before: " << d.getData() << endl;
      }
      else 
	std::cerr << "Data is of type " << d.getType() << endl;
      */
      char *buffer = scinew char[ d.getSize() ];
      HomogenousConvertHostToNetwork( (void *)buffer,
				      (void *)(d.getData()),
				      (DataTypes)d.getType(),
				      d.getSize() );
      
      bytesWritten = parent->connection.write( buffer,
					       d.getSize() ); 
      
      delete buffer;
    }
    else 
      
      // Write the data to our connection
      bytesWritten = parent->connection.write( d.getData(),
					       d.getSize() );
    
      std::cerr << "Wrote " << bytesWritten << " bytes." << endl;
    //std::cerr << "Done writing" << endl;
    // If there's an error, log it and move on.
  

    if ( bytesWritten != d.getSize() ) {
      char buffer[1000];
      
      snprintf( buffer, 100,
		"Write error - wrote %d/%d bytes on connection %s. Error = %s",
		bytesWritten, d.getSize(), parent->name, strerror(errno) );
      Log::log( Logging::ERROR, buffer );
      
      // Punt.
      parent->cleanDisconnect = false;
      parent->netMonitor->getMailbox().send( parent );
      Log::log( Logging::DEBUG, "Writer exiting" );
      done = true;
      return;
    }


    // Free memory if needed.
    d.purge();
    end = (double)times(&dummy)*ci;
    std::cerr << "Write took " << ( end - start ) << " seconds " << endl;
  }
  Log::log( Logging::DEBUG, "Writer exiting" );
  done = true;

  cerr << "End of NetConnectionWriter::run, thread id is " << pthread_self() << endl;
}



NetMonitor::NetMonitor( list<NetConnection*>& connectList,
			CrowdMonitor & connectListLock) :
  removeBox( "NetMonitorMailbox", MAX_PENDING_ITEMS ),
  connectList( connectList ),
  connectListLock( connectListLock )
{
  cerr << "In NetMonitor::NetMonitor, thread id is " << pthread_self() << endl;
  cerr << "End of NetMonitor::NetMonitor, thread id is " << pthread_self() << endl;
}

NetMonitor::~NetMonitor() {

}

void
NetMonitor::run() {
  cerr << "In NetMonitor::run, thread id is " << pthread_self() << endl;
  
  std::cerr << "NetMonitor is pid " << getpid() << endl;
  NetConnection * removeConnection;
  char buffer[1000];

  //SemotusVisum::Thread::Thread::self()->makeInterruptable();
  
  for( ;; ) {
    
    // Do we have connections that need to be disposed of?
    removeConnection = removeBox.receive();

    if ( removeConnection == NULL ) { // Remove ALL connections, PTP and MC
      NetInterface::getInstance().removeAllConnections();
      continue;
    }
    
    snprintf( buffer, 1000, "Removing conection to %s",
	      removeConnection->getName() );
    Log::log( Logging::MESSAGE, buffer );
    

    NetInterface::getInstance().removeConnection( removeConnection );

    
    snprintf( buffer, 1000, "We have %d connections left",
	      connectList.size() );
    Log::log( Logging::DEBUG, buffer );

    NetInterface::getInstance().netConnectionLock.writeLock();
    delete removeConnection;
    NetInterface::getInstance().netConnectionLock.writeUnlock();
    
    Log::log( Logging::DEBUG, "Deleted dead connection" );
  }
  
#if 0
    else 
      /* Check for data available, and notify the net connections. */
      NetConnection::notifyDataAvailable();
#endif
  cerr << "End of NetMonitor::run, thread id is " << pthread_self() << endl;
}

//////////
// Instantiation of incoming Thread.
Thread *
NetConnection::incomingThread = NULL;

//////////
// Instantiation of the list of all active connections.
  //list<NetConnection *>
  //NetConnection::connectionList;

//////////
// Instantiation of connection list lock.
  //CrowdMonitor
  //NetConnection::connectionListLock( "ConnectionListLock" );

//////////
// Instantiation of network monitor.
NetMonitor*
NetConnection::netMonitor = NULL;//( NetInterface::net->clientConnections,
//  NetInterface::net->netConnectionLock );

//////////
// Instantiation of dispatch manager usage flag.
bool
NetConnection::useDispatchManager = true;

//////////
// Instantiation of callback function (when dispatch manager is not
// in use.
void
(*NetConnection::callbackFunction)(void *) = NULL;
  
NetConnection::NetConnection( Connection &connection,
			      const char * name, int flags ) :
  
  nickname( NULL ),
  connection(connection),
  mbox( name, MAX_PENDING_ITEMS ), cleanDisconnect( false )
  
{
  cerr << "In NetConnection::NetConnection, thread id is " << pthread_self() << endl;
  if ( !netMonitor )
    netMonitor =
      scinew NetMonitor( NetInterface::getInstance().clientConnections,
			 NetInterface::getInstance().netConnectionLock );
    
    this->name = strdup(name);

  /* Set up incoming data thread */
  if ( incomingThread == NULL ) {
    incomingThread =
      scinew Thread( NetConnection::netMonitor,
		     "Network Monitor" );
    incomingThread->detach();
  }

  //std::cerr << "Constructor. This = " << (void *)this << endl;
  
  /* Create helpers and threads */
  char * threadName = NULL;
  if ( flags == READ_ONLY || flags == READ_WRITE ) {
    
    // Set the name 
    threadName = scinew char[ strlen( name ) + 2 ];
    sprintf( threadName, "%sR", name );
	     
    Reader = scinew NetConnectionReader( this );
    readThread  = scinew Thread( Reader, threadName );
#ifdef _USE_DETACH
    readThread->detach();
    readThread = NULL;
#endif
  }
  else {
    Reader = NULL; readThread = NULL;
  }
    
  if ( flags == WRITE_ONLY || flags == READ_WRITE ) {
    
    // Set the name 
    threadName = scinew char[ strlen( name ) + 2 ];
    sprintf( threadName, "%sW", name );
	     
    Writer = scinew NetConnectionWriter( this );
    writeThread = scinew Thread( Writer, threadName );
#ifdef _USE_DETACH
    writeThread->detach();
    writeThread = NULL;
#endif
  }
  else {
    Writer = NULL; writeThread = NULL;
  }
  
  /* Add ourselves to the list of network connections. */

  // Lock the list
  // connectionListLock.writeLock();
  
  // Add ourselves
  // connectionList.push_front( this );
  
  // Unlock the list
  // connectionListLock.writeUnlock();
  cerr << "End of NetConnection::NetConnection, thread id is " << pthread_self() << endl;
}


NetConnection::NetConnection( const NetConnection& netConnect) :
  
  nickname( netConnect.nickname ), name( netConnect.name ),
  connection( netConnect.connection ),
  mbox( netConnect.name, netConnect.mbox.size() )
  
{
  cerr << "In NetConnection::NetConnection, thread id is " << pthread_self() << endl;
  // When we copy a connection, we need not add it to the global list.
  std::cerr << "Copy constructor" << endl;
  cerr << "End of NetConnection::NetConnection, thread id is " << pthread_self() << endl;
}


NetConnection::~NetConnection() {
  cerr << "In NetConnection destructor, thread id is " << pthread_self() << endl;
  char buffer[1000];
  
  /* Wait until our helpers are done. */
  Log::log( Logging::DEBUG, "In nc destructor." );
  
  /* If this disconnect was clean, our reader is still running (blocked on
     data read). Kill it, and collect the thread (along with our knees ). */
#ifndef _USE_DETACH
  if ( readThread && !cleanDisconnect ) {
    Log::log( Logging::DEBUG, "Destroying read thread" );
    Reader->dieNow = true;
    //readThread->interrupt();
    
    readThread->join();
  }
  
  sleep(1); // Let the threads finish up.
#else
  if ( Reader && !cleanDisconnect ) {
    Log::log( Logging::DEBUG, "Destroying read thread" );
    Reader->dieNow = true;
  }
#endif
  //  if ( readThread ) {
  //  snprintf( buffer, 1000, "Address of read thread is %x",
  //  	      (void *)readThread );
  //  Log::log( Logging::DEBUG, buffer );
    //while ( !Reader->done ) sleep(1);
    //readThread->join();
  //}
  snprintf( buffer, 1000, "Joined read thread %s", name );
  Log::log( Logging::DEBUG, buffer );
  
  if ( Writer ) {
    Writer->dieNow = true;
    std::cerr << "Interrupting write thread" << endl;
    //writeThread->interrupt();
    std::cerr << "Interrupted write thread" << endl;
    dataItem di;
    mbox.send( di );
    while ( !Writer->done )
      Thread::yield(); // Give up time so the write thread can get it!
    
    //snprintf( buffer, 1000, "Address of write thread is %x",
    //	      (void *)writeThread );
    //Log::log( Logging::DEBUG, buffer );
    //while ( !Writer->done ) sleep(1);
#ifndef _USE_DETACH
    writeThread->join();
#endif
  }
  snprintf( buffer, 1000, "Joined write thread %s", name );
  Log::log( Logging::DEBUG, buffer );

  // TEST FOR LINUX 3-19-02 ejl delete &connection;
  //delete Reader;
  //delete Writer;

  /* We don't delete name right now for debugging purposes. It makes our
     name garbage to the thread lib. */
  //  delete name;
  //  delete nickname;
  Log::log( Logging::DEBUG, "Done with nc destructor" );
  cerr << "End of NetConnection destructor, thread id is " << pthread_self() << endl;
}

NetMonitor &
NetConnection::getNetMonitor() {
  cerr << "In NetConnection::getNetMonitor, thread id is " << pthread_self() << endl;
  if ( !netMonitor )
    netMonitor =
      scinew NetMonitor( NetInterface::getInstance().clientConnections,
			 NetInterface::getInstance().netConnectionLock );
  cerr << "End of NetConnection::getNetMonitor, thread id is " << pthread_self() << endl;
  return *netMonitor;
}

void
NetConnection::goodbyeCallback( void * obj, MessageData *input ) {
  cerr << "In NetConnection::goodbyeCallback, thread id is " << pthread_self() << endl;
  list<NetConnection*>::iterator i;
  NetConnection *nc = NULL;

  // Look for the connection that this callback belongs to.
  NetInterface::getInstance().netConnectionLock.readLock();
  for ( i = NetInterface::getInstance().clientConnections.begin();
	i != NetInterface::getInstance().clientConnections.end(); i++ ) {
    if ( !strcmp( input->clientName, (*i)->getName() ) ) {
      nc = *i;
      break;
    }
  }
  NetInterface::getInstance().netConnectionLock.readUnlock();

  // Huh? We got a goodbye from a client that doesn't exist? Probably some
  // sort of error (or the connection died while we were processing...)
  if ( nc == NULL ) {
    Log::log( Logging::WARNING, "Client not connected sent goodbye message");
  }
  else {
    char buffer[1000];
    snprintf(buffer,1000,"Goodbye to client %s", input->clientName );
    Log::log( Logging::DEBUG, buffer );
    nc->cleanDisconnect = true;
    netMonitor->getMailbox().send( nc );
  }
  cerr << "End of NetConnection::goodbyeCallback, thread id is " << pthread_self() << endl;
}

bool
NetConnection::operator==( const NetConnection &nc ) {
  cerr << "In NetConnection assignment, thread id is " << pthread_self() << endl;
  cerr << "End of NetConnection assignment, thread id is " << pthread_self() << endl;
  return true;
  /*  std::cerr << "Names: " << !strcmp( name, nc.name ) << endl;
  std::cerr << "Threads: " << (myThread == nc.myThread) << endl;
  return ( !strcmp( name, nc.name ) && myThread == nc.myThread );*/
}

#if 0
void
NetConnection::notifyDataAvailable() {
  cerr << "In NetConnection::notifyDataAvailable, thread id is " << pthread_self() << endl;
  connectionListLock.readLock();
  int numConnections = connectionList.size();
  connectionListLock.readUnlock();

  /* If we have no connections, return. */
  if ( numConnections == 0 ) return;

  /* Else, pull a list of connections with data ready */
  connectionListLock.readLock();
  Connection ** connections =
    connectionList.front()->getConnection().getReadyToRead();
  connectionListLock.readUnlock();

  if ( connections == NULL )
    // No data.
    return;

  list<NetConnection*>::iterator li;
  
  for ( int i = 0; connections[ i ] != NULL; i++ ) {
    
    /* For each connection, find the net connection associated with it. */
    NetConnection *nc = NULL;
#if 0
    unsigned int dataSize = 0;
#endif
    for ( li = connectionList.begin(), nc = NULL ;
	  li != connectionList.end(); li++ ) {
      if ( connections[ i ]->isEqual( (*li)->getConnection() ) ) {
	nc = (*li);
	break;
      }
						      
    }

    if ( nc == NULL )
      // Error - a connection without a network connection. Can't Happen!
      Log::log( Logging::ERROR,
		"Can't happen - raw connection without a network connection!");
    else {
        
      /* Only look for more data if we're not in the process of
	 reading now... */
      if ( nc->getMailbox().numItems() == 0 ) {
	dataItem *di = scinew dataItem( NULL, -1, false );
	Log::log( Logging::DEBUG, "Ready to read data from net" );
	if ( nc->getMailbox().trySend( *di ) == false )
	  delete di;
      }
    }
  }

  delete connections;
  cerr << "End of NetConnection::notifyDataAvailable, thread id is " << pthread_self() << endl;
}
#endif
int
NetConnection::write( const char * data, const int numBytes ) {
  cerr << "In NetConnection::write, thread id is " << pthread_self() << endl;
  Log::log( Logging::DEBUG,
	    "Writing this data to the network: ");
  Log::logBinary( Logging::DEBUG, data, numBytes );
  cerr << "End of NetConnection::write, thread id is " << pthread_self() << endl;		  
  return connection.write( data, numBytes );
}



}
}
