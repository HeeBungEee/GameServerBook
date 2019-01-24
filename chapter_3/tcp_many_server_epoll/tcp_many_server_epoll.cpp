#ifdef _WIN32
#include "../ImaysNet/ImaysNet.h"
#else
#include "../ImaysNetLinux/ImaysNet.h"

#endif
#include <stdio.h>
#include <signal.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>
#include <vector>
#include <iostream>
#include <mutex>




using namespace std;

// true�� �Ǹ� ���α׷��� �����մϴ�.
volatile bool stopWorking = false;


void ProcessSignalAction(int sig_number)
{
	if (sig_number == SIGINT)
		stopWorking = true;
}


int main()
{
	// ����ڰ� ctl-c�� ������ ���η����� �����ϰ� ����ϴ�.
	signal(SIGINT, ProcessSignalAction);

	try
	{
		// epoll�� ���� ���� Ŭ���̾�Ʈ�� �޾� ó���Ѵ�.

		// ���� ���� Ŭ�� tcp 5555�� ���� ������,
		// �׵��� hello world�� ������ ���� ���̴�.
		// �װ��� �׳� �������ֵ��� ����.
		// ���������� �� ó���� ����Ʈ���� ���������� ��¸� ����.
		
		// ���ɺ��ٴ� �ҽ� �������� �켱���� ������� �ֽ��ϴ�. 
		// ���� string ��ü�� ���ú��� ����,������ ��� ����
		// ���ɻ� �����Ǵ� �κ��� �˾Ƽ� �����ϰ� �����ø� �����Ͻʽÿ�.

		// TCP ���� ������ ��ü.
		struct RemoteClient
		{
			Socket tcpConnection;		// accept�� TCP ����
		};
		unordered_map<RemoteClient*, shared_ptr<RemoteClient>> remoteClients;

		// epoll�� �غ��Ѵ�.
		Epoll epoll;

		// TCP ������ �޴� ����
		Socket listenSocket(SocketType::Tcp);
		listenSocket.Bind(Endpoint("0.0.0.0", 5555));

		listenSocket.SetNonblocking();
		listenSocket.Listen();

		// epoll�� �߰��Ѵ�.
		epoll.Add(listenSocket, 
			nullptr,			/*���������� ���� RemoteClient�� �����Ƿ�*/
			EPOLLIN | EPOLLET);

		cout << "������ ���۵Ǿ����ϴ�.\n";
		cout << "CTL-CŰ�� ������ ���α׷��� �����մϴ�.\n";

		// ���� ���ϰ� TCP ���� ���� ��ο� ���ؼ� I/O ����(avail) �̺�Ʈ�� ���� ������ ��ٸ���. 
		// �׸��� ���� I/O ���� ���Ͽ� ���ؼ� ���� �Ѵ�.

		while (!stopWorking)
		{
			// I/O ���� �̺�Ʈ�� ���� ������ ��ٸ��ϴ�.
			EpollEvents readEvents;
			epoll.Wait(readEvents, 100/*Ÿ�Ӿƿ�*/);

			// ���� �̺�Ʈ ������ ó���մϴ�.
			for (int i = 0; i < readEvents.m_eventCount; i++)
			{
				auto& readEvent = readEvents.m_events[i];
				if (readEvent.data.ptr == 0) // ���������̸�
				{
					// ����Ʈ�����̹Ƿ� �� �̻� ���� �� �� ���� ������ �ݺ��ؾ� �մϴ�.
					// �ڼ��� ���� å�� 3.8���� �����ϼ���.
					while (true)
					{
						// accept�� ó���Ѵ�.
						auto remoteClient = make_shared<RemoteClient>();

						// �̹� "Ŭ���̾�Ʈ ������ ������" �̺�Ʈ�� �� �����̹Ƿ� �׳� �̰��� ȣ���ص� �ȴ�.
						string ignore;
						if (listenSocket.Accept(remoteClient->tcpConnection, ignore) != 0)
						{
							break; // would block Ȥ�� ��Ÿ �����̸� �ݺ��� �׸��Ѵ�.
						}

						remoteClient->tcpConnection.SetNonblocking();

						// epoll�� �߰��Ѵ�.
						epoll.Add(remoteClient->tcpConnection, remoteClient.get(), EPOLLIN | EPOLLET);

						// �� Ŭ���̾�Ʈ�� ��Ͽ� �߰�.
						remoteClients.insert({ remoteClient.get(), remoteClient });

						cout << "Client joined. There are " << remoteClients.size() << " connections.\n";
					}
				}
				else  // TCP ���� �����̸�
				{
					// ���� �����͸� �״�� ȸ���Ѵ�.
					shared_ptr<RemoteClient> remoteClient = remoteClients[(RemoteClient*)readEvent.data.ptr];
					if(remoteClient)
					{
						// ����Ʈ�����̹Ƿ� �� �̻� ���� �� �� ���� ������ �ݺ��ؾ� �մϴ�.
						// �ڼ��� ���� å�� 3.8���� �����ϼ���.
						while (true)
						{
							string data;
							int ec = remoteClient->tcpConnection.Receive();
							
							if (ec < 0 && errno == EWOULDBLOCK)
							{
								// would block�̸� �׳� �ݺ��� �ߴ�.
								break;
							}
							
							if (ec <= 0)
							{
								// ���� Ȥ�� ���� �����̴�.
								// �ش� ������ �����ع�����. 
								remoteClient->tcpConnection.Close();
								remoteClients.erase(remoteClient.get());

								cout << "Client left. There are " << remoteClients.size() << " connections.\n";
								break;
							}
							else
							{
								// ��Ģ��ζ�� TCP ��Ʈ�� Ư���� �Ϻθ� �۽��ϰ� �����ϴ� ��쵵 ����ؾ� �ϳ�,
								// ������ ������ ���ذ� �켱�̹Ƿ�, �����ϵ��� �Ѵ�.
								remoteClient->tcpConnection.Send(remoteClient->tcpConnection.m_receiveBuffer, ec);
							}
						}
					}
				}
			}
		}

		// ����ڰ� CTL-C�� ������ ������ ������. ��� ���Ḧ ����.
		listenSocket.Close();
		remoteClients.clear();
	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}

	return 0;
}

