clientrun: client.c utils.c # 编译详细输出内容的程序，并运行
	gcc -c utils.c
	gcc utils.o client.c -o client -lpthread
	sudo ./client
chatrun: chat.c utils.c # 编译用于测试聊天的程序，并运行
	gcc -c utils.c
	gcc utils.o  chat.c -o chat -lpthread
	sudo ./chat 
router1run: router1.c utils.c
	gcc -c utils.c
	gcc utils.o  router1.c -o router1 -lpthread
	sudo ./router1
router2run: router2.c utils.c
	gcc -c utils.c
	gcc utils.o  router2.c -o router2 -lpthread
	sudo ./router2
clean: # 清除所有的文件
	rm router1
	rm router2
	rm *.o
	rm chat
	rm client