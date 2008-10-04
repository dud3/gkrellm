/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  bsd-net-open.c code is Copyright (C):
|         Anthony Mallet <anthony.mallet@useless-ficus.net>
|
|
|  GKrellM is free software: you can redistribute it and/or modify it
|  under the terms of the GNU General Public License as published by
|  the Free Software Foundation, either version 3 of the License, or
|  (at your option) any later version.
|
|  GKrellM is distributed in the hope that it will be useful, but WITHOUT
|  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
|  License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with this program. If not, see http://www.gnu.org/licenses/
|
|
|  Additional permission under GNU GPL version 3 section 7
|
|  If you modify this program, or any covered work, by linking or
|  combining it with the OpenSSL project's OpenSSL library (or a
|  modified version of that library), containing parts covered by
|  the terms of the OpenSSL or SSLeay licenses, you are granted
|  additional permission to convey the resulting work.
|  Corresponding Source for a non-source form of such a combination
|  shall include the source code for the parts of OpenSSL used as well
|  as that of the covered work.
*/


/* ===================================================================== */
/* Inet monitor interface */

#if defined(__OpenBSD__) || ( defined(__NetBSD__) && __NetBSD_Version__ < 399000100 )

#include "../inet.h"

/* NO IPv6 SUPPORT YET */
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <kvm.h>

extern	kvm_t	*kvmd;


void
gkrellm_sys_inet_read_tcp_data(void)
{
   ActiveTCP tcp;
   gint	tcp_status;
   struct inpcbtable table;
   struct inpcb inpcb, *head, *next;
   struct tcpcb tcpcb;
   static struct nlist nl[] = {
#define X_TCBTABLE    0
   { "_tcbtable" },
   { NULL }
};


   if (kvmd == NULL) return;
   if (nl[0].n_type == 0)
      if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0)	return;

   if (kvm_read(kvmd, nl[X_TCBTABLE].n_value, (char *)&table,
		sizeof(struct inpcbtable)) != sizeof(struct inpcbtable))
      return;

	head = (struct inpcb *)
		&((struct inpcbtable *)nl[X_TCBTABLE].n_value)->inpt_queue.cqh_first;
   next = (struct inpcb *)table.inpt_queue.cqh_first;
   while(next != NULL && next != head) {
      
      if (kvm_read(kvmd, (u_long)next,
		   (char *)&inpcb, sizeof(inpcb)) == sizeof(inpcb) &&
	  kvm_read(kvmd, (u_long)inpcb.inp_ppcb,
		   (char *)&tcpcb, sizeof(tcpcb)) == sizeof(tcpcb)) {

	 tcp.local_port = ntohs(inpcb.inp_lport);
	 tcp.remote_addr.s_addr = inpcb.inp_faddr.s_addr;
	 tcp.remote_port = ntohs(inpcb.inp_fport);
	 tcp_status = (tcpcb.t_state == TCPS_ESTABLISHED);
	 tcp.family = AF_INET;
	 if (tcp_status == TCP_ALIVE)
	    gkrellm_inet_log_tcp_port_data(&tcp);
      }

      next = (struct inpcb *)inpcb.inp_queue.cqe_next;
   }
}
#endif

gboolean
gkrellm_sys_inet_init(void)
	{
	return TRUE;
	}
