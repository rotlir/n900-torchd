all:
	cc torchd.c -lgpiod -li2c -o torchd
	cc torch.c -o torch

install: torchd torch openrc/torchd
	install --owner=root torch /usr/local/bin
	install --owner=root torchd /usr/local/bin
	install --owner=root openrc/torchd /etc/init.d/

uninstall:
	rm /usr/local/bin/torch /usr/local/bin/torchd /etc/init.d/torchd

clean:
	rm torchd torch

