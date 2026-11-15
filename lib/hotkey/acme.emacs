include acme
ctl-x ctl-s	{test -e /mnt/acme/index || mount $acmesrv /mnt/acme; ~ $#acmewin 1 && echo put >/mnt/acme/$acmewin/ctl}
ctl-x k	{test -e /mnt/acme/index || mount $acmesrv /mnt/acme; ~ $#acmewin 1 && echo del >/mnt/acme/$acmewin/ctl}
ctl-x h	{echo cmd-a >/mnt/hotkey/send}
ctl-x u	{echo ctl-z >/mnt/hotkey/send}
ctl-_	{echo ctl-z >/mnt/hotkey/send}
ctl-f	{echo right >/mnt/hotkey/send}
ctl-b	{echo left >/mnt/hotkey/send}
ctl-n	{echo down >/mnt/hotkey/send}
ctl-p	{echo up >/mnt/hotkey/send}
ctl-o	{echo nl left >/mnt/hotkey/send}
ctl-w	{echo ctl-c bs >/mnt/hotkey/send}
ctl-y	{echo ctl-v >/mnt/hotkey/send}
ctl-k	{test -e /mnt/acme/index || mount $acmesrv /mnt/acme; ~ $#acmewin 1 && cd /mnt/acme/$acmewin && {echo -n 'addr=dot' >ctl; echo -n '.,/$/' >addr; a=`{cat addr}; ~ $a(1) $a(2) && echo -n '.,+#1' >addr; echo -n 'dot=addr' >ctl} <[3]addr && echo ctl-c bs >/mnt/hotkey/send}
