tcp-intercept
=============

This program is designed to intercept TCP connections passing through this box.
This can be set up using the TPROXY-target of iptables, together with the
appropriate `ip route` setup. See kernel/Documentation/networking/tproxy.txt for
details.


Rationale
---------

The idea is to perform TCP-acceleration on unaware endpoints. This acceleration
can consist of using MultiPath-TCP, using another congestion control algorithm,
or both.

Other projects exist to simply add MPTCP-support to existing connections on the
netfilter-layer (e.g.
http://www.ietf.org/mail-archive/web/multipathtcp/current/msg01934.html). This
is more performant, since it doesn't maintain any buffers.
In order to change the congestion control however, intermediate buffers are
needed.


Compiling and installing
------------------------

If you use a Debian-based system, you can build a `.deb` file by running
`dpkg-buildpackage -us -uc`, and install it with `dpkg -i generated-file.deb`.
You need to have the dependencies installed before you can build.

Dependencies:

* libev (`libev-dev` on debian-based systems)
* libdaemon (`libdaemon-dev` on debian-based systems)
* ptr_list from libboost (`libboost-dev` on debian-based systems)
* libsimplelog (https://github.com/VRT-onderzoek-en-innovatie/libsimplelog)

Instructions:

```
autopoint
autoreconf -i
./configure
make
sudo make install
```

TCP keepalive support
------------------------
Inorder to benefit from the TCP-keepalive functionality, you need to enable 
kernel support. Without this functionality, tcp-intercept has no way of 
cleaning up connections that hang becasue one of the peers lost connectivity 
without tcp-inntercept being aware of it. Should you wish to have this feature 
enabled (default is disabled), please start tcp-intercept with the keepalive 
option (-k).

You may use the procfs or sysctl interface to configure the kernel parameters.

procfs:
 * /proc/sys/net/ipv4/tcp_keepalive_time
 * /proc/sys/net/ipv4/tcp_keepalive_intvl
 * /proc/sys/net/ipv4/tcp_keepalive_probes
 
sysctl:
 * net.ipv4.tcp_keepalive_time
 * net.ipv4.tcp_keepalive_intvl
 * net.ipv4.tcp_keepalive_probes


TCP_NODELAY (no Nagel) support
-----------------------
tcp-intercept can disable (optionally) the Nagel-Algorithm used by TCP. TCP by default 
tries to optimise utilization of the network. In doing so, it buffers data segments 
(smaller than an MSS), until it reaches the MSS or it receives an ACK message for its 
sent data segments, and sends them out as one big segment. This way it avoids the 
frequent sending of small segments that could waste bandwidth. On the contrary, this 
could have a negative effect on latency/jitter sensitive applications (real time apps). 
Should you wish to have this feature enabled (default is disabled), please start 
tcp-intercept with the TCP_NODELAY option (-n).

iptables setup
-------------
```
IP_ROUTE_TABLE_NUMBER=5 # free routing table number to use
FWMARK="0x01/0x01" # Distinguishing FWmark bit to use
LISTEN_PORT=5000 # Port where tcp-intercept is listening on

# First add a custom route table that delivers everything locally
ip rule add fwmark $FWMARK table $IP_ROUTE_TABLE_NUMBER
ip route add local 0.0.0.0/0 dev lo table $IP_ROUTE_TABLE_NUMBER

iptables -t mangle -N tproxy

# If this packet matches an existing, open local socket, deliver it locally
iptables -t mangle -A tproxy -p tcp -m socket -j MARK --set-mark $FWMARK
iptables -t mangle -A tproxy -p tcp -m socket -j RETURN
# If it is a packet with a local destination, don't touch it
iptables -t mangle -A tproxy -p tcp -m addrtype --dst-type LOCAL -j RETURN
# Intercept all other TCP-connections
iptables -t mangle -A tproxy -p tcp -j TPROXY \
                   --on-port $LISTEN_PORT --tproxy-mark $FWMARK

iptables -t mangle -A PREROUTING -j tproxy
```
