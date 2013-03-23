# COPYRIGHT AND DISCLAIMER
# 
# Copyright (C) 1996-1997 by the Regents of the University of California.
# Copyright (c) 1999 by the University of Southern California
#
# IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
# DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
# OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
# EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT. THIS SOFTWARE IS
# PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO
# OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
# MODIFICATIONS.
#
# Taken from the UCB Home-IP trace by Steve Gribble <gribble@cs.berkeley.edu>
#
# $Header: /var/lib/cvs/ns-2.29/indep-utils/webtrace-conv/ucb/Makefile.in,v 1.1.1.1 2006/03/08 13:52:47 rouil Exp $

# Top level hierarchy
prefix  = /usr/local
# Pathname of directory to install the binary
BINDEST = /usr/local/bin

CC = g++
MKDEP	= ../../../conf/mkdep

INCLUDE = -I. -I/home/eric/ns/ns-allinone-2.29/tclcl-1.17 -I/home/eric/ns/ns-allinone-2.29/otcl-1.11 -I/home/eric/ns/ns-allinone-2.29/include -I/home/eric/ns/ns-allinone-2.29/include -I/usr/include/pcap
CFLAGS =  -Wall -DCPP_NAMESPACE=std
LDFLAGS = 
LIBS = -L/home/eric/ns/ns-allinone-2.29/lib -ltcl8.4  -lnsl -ldl -lm 
INSTALL = /usr/bin/install -c

SRC = tr-stat.cc logparse.cc utils.cc
OBJ = $(SRC:.cc=.o)

all: ucb-tr-stat

ucb-tr-stat: $(OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

install: ucb-tr-stat
	$(INSTALL) -m 555 -o bin -g bin ucb-tr-stat $(DESTDIR)$(BINDEST)

.SUFFIXES: .cc

.cc.o: 
	@rm -f $@
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $*.cc

clean:
	-/bin/rm -f *.o *~ ucb-tr-stat *core

FORCE:
	;

depend: $(SRC)
	$(MKDEP) $(CFLAGS) $(INCLUDE) $(SRC)
