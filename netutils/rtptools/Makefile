############################################################################
# apps/netutils/rtptools/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

RTPTOOLS_UNPACK   = rtptools
RTPTOOLS_URL      = https://github.com/irtlab/rtptools.git
RTPTOOLS_SRCDIR   = $(RTPTOOLS_UNPACK)

ifeq ($(NETUTILS_RTPTOOLS_SPECIFIC_VERSION),y)
RTPTOOLS_VERSION = $(CONFIG_NETUTILS_RTPTOOLS_VERSION)
else
RTPTOOLS_VERSION = "master"
endif

DEPPATH += --dep-path $(RTPTOOLS_SRCDIR)
VPATH   += :$(RTPTOOLS_SRCDIR)

CSRCS += multimer.c
CSRCS += notify.c
CSRCS += payload.c
CSRCS += rd.c
CSRCS += utils.c

ifeq ($(CONFIG_ENDIAN_BIG),y)
RTP_BIG_ENDIAN = 1
else
RTP_BIG_ENDIAN = 0
endif

$(RTPTOOLS_UNPACK):
	$(Q) echo "git clone $(RTPTOOLS_URL) $(RTPTOOLS_UNPACK)"
	$(Q) git clone $(RTPTOOLS_URL) $(RTPTOOLS_UNPACK)
	$(Q) git -C $(RTPTOOLS_UNPACK) checkout $(RTPTOOLS_VERSION)
	$(Q) git -C $(RTPTOOLS_UNPACK) reset --hard
	$(Q) echo "#define RTP_BIG_ENDIAN $(RTP_BIG_ENDIAN)" >> $(RTPTOOLS_UNPACK)/config.h
	$(Q) echo "#define HAVE_MSGCONTROL 1" >> $(RTPTOOLS_UNPACK)/config.h

context:: $(RTPTOOLS_UNPACK)

distclean::
	$(call DELDIR, $(RTPTOOLS_UNPACK))

# Configuration Applications

ifneq ($(CONFIG_RTPTOOLS_APPS),)

MODULE = $(CONFIG_RTPTOOLS_APPS)

ifeq ($(CONFIG_NETUTILS_RTPTOOLS_RTPPLAY),y)

PROGNAME  += $(CONFIG_NETUTILS_RTPTOOLS_RTPPLAY_PROGNAME)
PRIORITY  += $(CONFIG_NETUTILS_RTPTOOLS_RTPPLAY_PRIORITY)
STACKSIZE += $(CONFIG_NETUTILS_RTPTOOLS_RTPPLAY_STACKSIZE)

MAINSRC += $(RTPTOOLS_UNPACK)/rtpplay.c
endif

ifeq ($(CONFIG_NETUTILS_RTPTOOLS_RTPSEND),y)

PROGNAME  += $(CONFIG_NETUTILS_RTPTOOLS_RTPSEND_PROGNAME)
PRIORITY  += $(CONFIG_NETUTILS_RTPTOOLS_RTPSEND_PRIORITY)
STACKSIZE += $(CONFIG_NETUTILS_RTPTOOLS_RTPSEND_STACKSIZE)

MAINSRC += $(RTPTOOLS_UNPACK)/rtpsend.c
endif

ifeq ($(CONFIG_NETUTILS_RTPTOOLS_RTPDUMP),y)

PROGNAME  += $(CONFIG_NETUTILS_RTPTOOLS_RTPDUMP_PROGNAME)
PRIORITY  += $(CONFIG_NETUTILS_RTPTOOLS_RTPDUMP_PRIORITY)
STACKSIZE += $(CONFIG_NETUTILS_RTPTOOLS_RTPDUMP_STACKSIZE)

MAINSRC += $(RTPTOOLS_UNPACK)/rtpdump.c
endif

ifeq ($(CONFIG_NETUTILS_RTPTOOLS_RTPTRANS),y)

PROGNAME  += $(CONFIG_NETUTILS_RTPTOOLS_RTPTRANS_PROGNAME)
PRIORITY  += $(CONFIG_NETUTILS_RTPTOOLS_RTPTRANS_PRIORITY)
STACKSIZE += $(CONFIG_NETUTILS_RTPTOOLS_RTPTRANS_STACKSIZE)

MAINSRC += $(RTPTOOLS_UNPACK)/rtptrans.c
endif

endif

include $(APPDIR)/Application.mk
