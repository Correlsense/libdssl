/*
** This file is a part of DSSL library.
**
** Copyright (C) 2005-2009, Atomic Labs, Inc.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
*/
#include <string.h>
#include "stdinc.h"
#include "decode.h"

void pcap_cb_ethernet( u_char *ptr, const struct pcap_pkthdr *header, const u_char *pkt_data );
void pcap_cb_sll( u_char *ptr, const struct pcap_pkthdr *header, const u_char *pkt_data );

#ifndef DSSL_NO_PCAP
pcap_handler GetPcapHandler( pcap_t* p )
{
	pcap_handler rc = NULL;
	int dlink = 0;
	if( !p ) { _ASSERT( FALSE ); return NULL; }

	dlink = pcap_datalink( p );
	switch( dlink )
	{
		case DLT_EN10MB: rc = pcap_cb_ethernet; break;
#ifdef DLT_LINUX_SLL
                case DLT_LINUX_SLL: rc = pcap_cb_sll; break;
#endif
		default:
			/*Unsupported link type*/
			rc = NULL;
			break;
	}

	return rc;
}
#endif


#ifdef DLT_LINUX_SLL

#ifndef SLL_HDR_LEN
#define SLL_HDR_LEN     16              /* total header length */
#endif

#ifndef SLL_ADDRLEN
#define SLL_ADDRLEN     8               /* length of address field */
#endif

struct datalink_sll_header {
        uint16_t sll_pkttype;          /* packet type */
        uint16_t sll_hatype;           /* link-layer address type */
        uint16_t sll_halen;            /* link-layer address length */
        uint8_t sll_addr[SLL_ADDRLEN]; /* link-layer address */
        uint16_t sll_protocol;         /* protocol */
};

void pcap_cb_sll( u_char *ptr, const struct pcap_pkthdr *header, const u_char *pkt_data )
{
        CapEnv* env = (CapEnv*)ptr;
        DSSL_Pkt packet;
        int len = header->caplen;
        struct datalink_sll_header *sll_header = (struct datalink_sll_header *)pkt_data;

#ifdef NM_TRACE_FRAME_COUNT
        DEBUG_TRACE1("\n-=LINUX-SLL-FRAME: %u", env->frame_cnt);
        ++env->frame_cnt;
#endif

        memset( &packet, 0, sizeof( packet ) );
        memcpy( &packet.pcap_header, header, sizeof(packet.pcap_header) );

        packet.pcap_ptr = pkt_data;
        packet.ether_header = (struct ether_header*) pkt_data;

        if( len < SLL_HDR_LEN )
        {
                nmLogMessage( ERR_CAPTURE, "pcap_cb_sll: Invalid SLL header length!" );
                return;
        }

        if( ntohs(sll_header->sll_protocol) == ETHERTYPE_IP )
        {
                DecodeIpPacket( env, &packet, pkt_data + SLL_HDR_LEN, len - SLL_HDR_LEN );
        }

}
#endif

#ifndef ETHERTYPE_VLAN
#define ETHERTYPE_VLAN 0x8100
#endif

void pcap_cb_ethernet( u_char *ptr, const struct pcap_pkthdr *header, const u_char *pkt_data )
{
	CapEnv* env = (CapEnv*)ptr;
	DSSL_Pkt packet;
	int len = header->caplen;
	int m_link_protocol_offset = 12;
	int m_link_len = ETHER_HDRLEN;
	int pkt_link_len = m_link_len;

#ifdef NM_TRACE_FRAME_COUNT
	DEBUG_TRACE1("\n-=ETH-FRAME: %u", env->frame_cnt);
	++env->frame_cnt;
#endif

	memset( &packet, 0, sizeof( packet ) );
	memcpy( &packet.pcap_header, header, sizeof(packet.pcap_header) );

	packet.pcap_ptr = pkt_data;
	packet.link_type = pcap_datalink(env->pcap_adapter);

	packet.ether_header = (struct ether_header*) pkt_data;

	if( len < ETHER_HDRLEN )
	{
		nmLogMessage( ERR_CAPTURE, "pcap_cb_ethernet: Invalid ethernet header length!" );
		return;
	}

	if (pkt_data[m_link_protocol_offset]!=0x08 || pkt_data[m_link_protocol_offset+1]!=0x00) {
		if ( pkt_data[m_link_protocol_offset]==0x81 && pkt_data[m_link_protocol_offset+1]==0x00		// is vlan packet
			&& pkt_data[m_link_protocol_offset+4]==0x08 && pkt_data[m_link_protocol_offset+5]==0x00)	// AND is IP packet
		{
			// adjust for vlan (801.1q) packet headers
			pkt_link_len += 4;
		} else {
			// not an ethernet packet or non-IP vlan packet
			return;
		}
	}
//	if( ntohs(packet.ether_header->ether_type) == ETHERTYPE_IP )
	{
		DecodeIpPacket( env, &packet, pkt_data + pkt_link_len, len - ETHER_HDRLEN );
	}
}
