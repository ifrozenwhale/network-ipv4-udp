project: client.c chat.c utils.o # 编译整个工程所有的文件
	gcc utils.o client.c -o client -lpthread
	gcc utils.o chat.c -o chat -lpthread
	gcc -c utils.c 
clientrun: client.c utils.c # 编译详细输出内容的程序，并运行
	gcc -c utils.c
	gcc utils.o client.c -o client -lpthread
	sudo ./client
chatrun: chat.c utils.c # 编译用于测试聊天的程序，并运行
	gcc -c utils.c
	gcc utils.o  chat.c -o chat -lpthread
	sudo ./chat 
clean: # 清除所有的文件
	rm client
	rm chat
	rm utils.o