#include <event/event_callback.h>
#include <event/event_main.h>
#include <event/event_system.h>

#include <io/net/tcp_server.h>

#include <io/pipe/pipe.h>
#include <io/pipe/pipe_producer.h>
#include <io/pipe/splice.h>

#include <io/socket/simple_server.h>

#include <ssh/ssh_protocol.h>
#include <ssh/ssh_transport_pipe.h>

class SSHConnection {
	LogHandle log_;
	Socket *peer_;
	SSHTransportPipe *pipe_;
	Action *receive_action_;
	Splice *splice_;
	Action *splice_action_;
	Action *close_action_;
public:
	SSHConnection(Socket *peer)
	: log_("/ssh/connection"),
	  peer_(peer),
	  pipe_(NULL),
	  splice_(NULL),
	  splice_action_(NULL),
	  close_action_(NULL)
	{
		pipe_ = new SSHTransportPipe();
		EventCallback *rcb = callback(this, &SSHConnection::receive_complete);
		receive_action_ = pipe_->receive(rcb);

		splice_ = new Splice(log_ + "/splice", peer_, pipe_, peer_);
		EventCallback *cb = callback(this, &SSHConnection::splice_complete);
		splice_action_ = splice_->start(cb);
	}

	~SSHConnection()
	{
		ASSERT(close_action_ == NULL);
		ASSERT(splice_action_ == NULL);
		ASSERT(splice_ == NULL);
		ASSERT(receive_action_ == NULL);
		ASSERT(pipe_ == NULL);
		ASSERT(peer_ == NULL);
	}

private:
	void receive_complete(Event e)
	{
		receive_action_->cancel();
		receive_action_ = NULL;

		switch (e.type_) {
		case Event::Done:
			break;
		default:
			ERROR(log_) << "Unexpected event while waiting for a packet: " << e;
			return;
		}

		ASSERT(!e.buffer_.empty());

		/*
		 * SSH Echo!
		 */
		pipe_->send(&e.buffer_);

		EventCallback *rcb = callback(this, &SSHConnection::receive_complete);
		receive_action_ = pipe_->receive(rcb);
	}

	void close_complete(void)
	{
		close_action_->cancel();
		close_action_ = NULL;

		ASSERT(peer_ != NULL);
		delete peer_;
		peer_ = NULL;

		delete this;
	}

	void splice_complete(Event e)
	{
		splice_action_->cancel();
		splice_action_ = NULL;

		switch (e.type_) {
		case Event::EOS:
			DEBUG(log_) << "Peer exiting normally.";
			break;
		case Event::Error:
			ERROR(log_) << "Peer exiting with error: " << e;
			break;
		default:
			ERROR(log_) << "Peer exiting with unknown event: " << e;
			break;
		}

	        ASSERT(splice_ != NULL);
		delete splice_;
		splice_ = NULL;

		if (receive_action_ != NULL) {
			INFO(log_) << "Peer exiting while waiting for a packet.";

			receive_action_->cancel();
			receive_action_ = NULL;
		}

		ASSERT(pipe_ != NULL);
		delete pipe_;
		pipe_ = NULL;

		ASSERT(close_action_ == NULL);
		SimpleCallback *cb = callback(this, &SSHConnection::close_complete);
		close_action_ = peer_->close(cb);
	}
};

class SSHServer : public SimpleServer<TCPServer> {
public:
	SSHServer(SocketAddressFamily family, const std::string& interface)
	: SimpleServer<TCPServer>("/ssh/server", family, interface)
	{ }

	~SSHServer()
	{ }

	void client_connected(Socket *client)
	{
		new SSHConnection(client);
	}
};


int
main(void)
{
	new SSHServer(SocketAddressFamilyIP, "[::]:2299");
	event_main();
}
