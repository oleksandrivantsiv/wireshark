/* packet-wtp.c
 *
 * Routines to dissect WTP component of WAP traffic.
 *
 * $Id: packet-wtp.c,v 1.45 2003/04/20 11:36:16 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
 *
 * WAP dissector based on original work by Ben Fowler
 * Updated by Neil Hunter <neil.hunter@energis-squared.com>
 * WTLS support by Alexandre P. Ferreira (Splice IP)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef NEED_SNPRINTF_H
# include "snprintf.h"
#endif

#include <string.h>
#include <glib.h>
#include <epan/packet.h>
#include "reassemble.h"
#include "packet-wap.h"
#include "packet-wtp.h"
#include "packet-wsp.h"

static const true_false_string continue_truth = {
    "TPI Present" ,
    "No TPI"
};

static const true_false_string RID_truth = {
    "Re-Transmission",
    "First transmission"
};

static const true_false_string TIDNew_truth = {
    "TID is new" ,
    "TID is valid"
};

static const true_false_string tid_response_truth = {
    "Response" ,
    "Original"
};

static const true_false_string UP_truth = {
    "User Acknowledgement required" ,
    "User Acknowledgement optional"
};

static const true_false_string TVETOK_truth = {
    "True",
    "False"
};

static const value_string vals_pdu_type[] = {
    { 0, "Not Allowed" },
    { 1, "Invoke" },
    { 2, "Result" },
    { 3, "Ack" },
    { 4, "Abort" },
    { 5, "Segmented Invoke" },
    { 6, "Segmented Result" },
    { 7, "Negative Ack" },
    { 0, NULL }
};

static const value_string vals_transaction_trailer[] = {
    { 0, "Not last packet" },
    { 1, "Last packet of message" },
    { 2, "Last packet of group" },
    { 3, "Re-assembly not supported" },
    { 0, NULL }
};

static const value_string vals_version[] = {
    { 0, "Current" },
    { 1, "Undefined" },
    { 2, "Undefined" },
    { 3, "Undefined" },
    { 0, NULL }
};

static const value_string vals_abort_type[] = {
    { 0, "Provider" },
    { 1, "User (WSP)" },
    { 0, NULL }
};

static const value_string vals_abort_reason_provider[] = {
    { 0x00, "Unknown" },
    { 0x01, "Protocol Error" },
    { 0x02, "Invalid TID" },
    { 0x03, "Not Implemented Class 2" },
    { 0x04, "Not Implemented SAR" },
    { 0x05, "Not Implemented User Acknowledgement" },
    { 0x06, "WTP Version Zero" },
    { 0x07, "Capacity Temporarily Exceeded" },
    { 0x08, "No Response" },
    { 0x09, "Message Too Large" },
    { 0x00, NULL }
};

static const value_string vals_transaction_classes[] = {
    { 0x00, "Unreliable Invoke without Result" },
    { 0x01, "Reliable Invoke without Result" },
    { 0x02, "Reliable Invoke with Reliable Result" },
    { 0x00, NULL }
};

static const value_string vals_tpi_type[] = {
    { 0x00, "Error" },
    { 0x01, "Info" },
    { 0x02, "Option" },
    { 0x03, "Packet sequence number" },
    { 0x04, "SDU boundary" },
    { 0x05, "Frame boundary" },
    { 0x00, NULL }
};

static const value_string vals_tpi_opt[] = {
    { 0x01, "Maximum receive unit" },
    { 0x02, "Total message size" },
    { 0x03, "Delay transmission timer" },
    { 0x04, "Maximum group" },
    { 0x05, "Current TID" },
    { 0x06, "No cached TID" },
    { 0x00, NULL }
};

/* File scoped variables for the protocol and registered fields */
static int proto_wtp 				= HF_EMPTY;

/* These fields used by fixed part of header */
static int hf_wtp_header_sub_pdu_size 		= HF_EMPTY;
static int hf_wtp_header_flag_continue 		= HF_EMPTY;
static int hf_wtp_header_pdu_type 		= HF_EMPTY;
static int hf_wtp_header_flag_Trailer 		= HF_EMPTY;
static int hf_wtp_header_flag_RID 		= HF_EMPTY;
static int hf_wtp_header_flag_TID 		= HF_EMPTY;
static int hf_wtp_header_flag_TID_response 	= HF_EMPTY;

/* These fields used by Invoke packets */
static int hf_wtp_header_Inv_version 		= HF_EMPTY;
static int hf_wtp_header_Inv_flag_TIDNew 	= HF_EMPTY;
static int hf_wtp_header_Inv_flag_UP	 	= HF_EMPTY;
static int hf_wtp_header_Inv_Reserved	 	= HF_EMPTY;
static int hf_wtp_header_Inv_TransactionClass 	= HF_EMPTY;


static int hf_wtp_header_variable_part 		= HF_EMPTY;
static int hf_wtp_data 				= HF_EMPTY;

static int hf_wtp_tpi_type	 		= HF_EMPTY;
static int hf_wtp_tpi_psn	 		= HF_EMPTY;
static int hf_wtp_tpi_opt	 		= HF_EMPTY;
static int hf_wtp_tpi_optval	 		= HF_EMPTY;
static int hf_wtp_tpi_info	 		= HF_EMPTY;

static int hf_wtp_header_Ack_flag_TVETOK	= HF_EMPTY;
static int hf_wtp_header_Abort_type		= HF_EMPTY;
static int hf_wtp_header_Abort_reason_provider	= HF_EMPTY;
static int hf_wtp_header_Abort_reason_user	= HF_EMPTY;
static int hf_wtp_header_sequence_number	= HF_EMPTY;
static int hf_wtp_header_missing_packets	= HF_EMPTY;

/* These fields used when reassembling WTP fragments */
static int hf_wtp_fragments			= HF_EMPTY;
static int hf_wtp_fragment			= HF_EMPTY;
static int hf_wtp_fragment_overlap		= HF_EMPTY;
static int hf_wtp_fragment_overlap_conflict	= HF_EMPTY;
static int hf_wtp_fragment_multiple_tails	= HF_EMPTY;
static int hf_wtp_fragment_too_long_fragment	= HF_EMPTY;
static int hf_wtp_fragment_error		= HF_EMPTY;

/* Initialize the subtree pointers */
static gint ett_wtp 				= ETT_EMPTY;
static gint ett_header 				= ETT_EMPTY;
static gint ett_tpilist 			= ETT_EMPTY;
static gint ett_wsp_fragments			= ETT_EMPTY;
static gint ett_wtp_fragment			= ETT_EMPTY;

static const fragment_items wtp_frag_items = {
    &ett_wtp_fragment,
    &ett_wsp_fragments,
    &hf_wtp_fragments,
    &hf_wtp_fragment,
    &hf_wtp_fragment_overlap,
    &hf_wtp_fragment_overlap_conflict,
    &hf_wtp_fragment_multiple_tails,
    &hf_wtp_fragment_too_long_fragment,
    &hf_wtp_fragment_error,
    NULL,
    "fragments"
};

/* Handle for WSP dissector */
static dissector_handle_t wsp_handle;

/*
 * reassembly of WSP
 */
static GHashTable	*wtp_fragment_table = NULL;

static void
wtp_defragment_init(void)
{
    fragment_table_init(&wtp_fragment_table);
}

/*
 * Extract some bitfields
 */
#define pdu_type(octet)			(((octet) >> 3) & 0x0F)	/* Note pdu type must not be 0x00 */
#define transaction_class(octet)	((octet) & 0x03)	/* ......XX */
#define transmission_trailer(octet)	(((octet) >> 1) & 0x01)	/* ......X. */

static char retransmission_indicator(unsigned char octet)
{
    switch (pdu_type(octet)) {
	case INVOKE:
	case RESULT:
	case ACK:
	case SEGMENTED_INVOKE:
	case SEGMENTED_RESULT:
	case NEGATIVE_ACK:
	    return octet & 0x01;	/* .......X */
	default:
	    return 0;
    }
}

/*
 * dissect a TPI
 */
static void
wtp_handle_tpi(proto_tree *tree, tvbuff_t *tvb)
{
    int			 offset = 0;
    unsigned char	 tByte;
    unsigned char	 tType;
    unsigned char	 tLen;
    proto_item     	*subTree = NULL;

    tByte = tvb_get_guint8(tvb, offset++);
    tType = (tByte & 0x78) >> 3;
    if (tByte & 0x04)				/* Long TPI	*/
	tLen = tvb_get_guint8(tvb, offset++);
    else
	tLen = tByte & 0x03;
    subTree = proto_tree_add_uint(tree, hf_wtp_tpi_type,
				  tvb, 0, tvb_length(tvb), tType);
    proto_item_add_subtree(subTree, ett_tpilist);
    switch (tType) {
	case 0x00:			/* Error*/
	    /* \todo	*/
	    break;
	case 0x01:			/* Info	*/
	    /* Beware, untested case here	*/
	    proto_tree_add_item(subTree, hf_wtp_tpi_info,
				tvb, offset, tLen, bo_little_endian);
	    break;
	case 0x02:			/* Option	*/
	    proto_tree_add_item(subTree, hf_wtp_tpi_opt,
				tvb, offset++, 1, bo_little_endian);
	    proto_tree_add_item(subTree, hf_wtp_tpi_optval,
				tvb, offset, tLen - 1, bo_little_endian);
	    break;
	case 0x03:			/* PSN	*/
	    proto_tree_add_item(subTree, hf_wtp_tpi_psn,
				tvb, offset, 1, bo_little_endian);
	    break;
	case 0x04:			/* SDU boundary	*/
	    /* \todo	*/
	    break;
	case 0x05:			/* Frame boundary	*/
	    /* \todo	*/
	    break;
	default:
	    break;
    }
}

/* Code to actually dissect the packets */
static void
dissect_wtp_common(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    char	szInfo[50];
    int		offCur		= 0; /* current offset from start of WTP data */

    unsigned char  b0;

    /* continuation flag */
    unsigned char  	fCon;			/* Continue flag	*/
    unsigned char  	fRID;			/* Re-transmission indicator*/
    unsigned char  	fTTR = '\0';		/* Transmission trailer	*/
    guint 		cbHeader   	= 0;	/* Fixed header length	*/
    guint 		vHeader   	= 0;	/* Variable header length*/
    int 		abortType  	= 0;

    /* Set up structures we'll need to add the protocol subtree and manage it */
    proto_item		*ti;
    proto_tree		*wtp_tree = NULL;

    char		pdut;
    char		clsTransaction 	= ' ';
    int			cchInfo;
    int			numMissing = 0;		/* Number of missing packets in a negative ack */
    int			i;
    tvbuff_t		*wsp_tvb = NULL;
    fragment_data	*fd_head = NULL;
    guint8		psn = 0;		/* Packet sequence number*/
    guint16		TID = 0;		/* Transaction-Id	*/

    b0 = tvb_get_guint8 (tvb, offCur + 0);
    /* Discover Concatenated PDUs */
    if (b0 == 0) {
	guint	c_fieldlen = 0;		/* Length of length-field	*/
	guint	c_pdulen = 0;		/* Length of conc. PDU	*/

	if (tree) {
	    ti = proto_tree_add_item(tree, proto_wtp,
				    tvb, offCur, 1, bo_little_endian);
	    wtp_tree = proto_item_add_subtree(ti, ett_wtp);
	}
	offCur = 1;
	i = 1;
	while (offCur < (int) tvb_reported_length(tvb)) {
	    b0 = tvb_get_guint8(tvb, offCur + 0);
	    if (b0 & 0x80) {
		c_fieldlen = 2;
		c_pdulen = ((b0 & 0x7f) << 8) | tvb_get_guint8(tvb, offCur + 1);
	    } else {
		c_fieldlen = 1;
		c_pdulen = b0;
	    }
	    if (tree) {
		proto_tree_add_item(wtp_tree, hf_wtp_header_sub_pdu_size,
				    tvb, offCur, c_fieldlen, bo_big_endian);
	    }
	    if (i > 1 && check_col(pinfo->cinfo, COL_INFO)) {
		col_append_str(pinfo->cinfo, COL_INFO, ", ");
	    }
	    wsp_tvb = tvb_new_subset(tvb, offCur + c_fieldlen, -1, c_pdulen);
	    dissect_wtp_common(wsp_tvb, pinfo, wtp_tree);
	    offCur += c_fieldlen + c_pdulen;
	    i++;
	}
	return;
    }
    fCon = b0 & 0x80;
    fRID = retransmission_indicator(b0);
    pdut = pdu_type(b0);

    /* Develop the string to put in the Info column */
    cchInfo = snprintf(szInfo, sizeof( szInfo ), "WTP %s",
		    val_to_str(pdut, vals_pdu_type, "Unknown PDU type 0x%x"));

    switch (pdut) {
	case INVOKE:
	    fTTR = transmission_trailer(b0);
	    TID = tvb_get_ntohs(tvb, offCur + 1);
	    psn = 0;
	    clsTransaction = transaction_class(tvb_get_guint8(tvb, offCur + 3));
	    snprintf(szInfo + cchInfo, sizeof(szInfo) - cchInfo,
		     " Class %d", clsTransaction);
	    cbHeader = 4;
	    break;

	case SEGMENTED_INVOKE:
	case SEGMENTED_RESULT:
	    fTTR = transmission_trailer(b0);
	    TID = tvb_get_ntohs(tvb, offCur + 1);
	    psn = tvb_get_guint8(tvb, offCur + 3);
	    cbHeader = 4;
	    break;

	case ABORT:
	    cbHeader = 4;
	    break;

	case RESULT:
	    fTTR = transmission_trailer(b0);
	    TID = tvb_get_ntohs(tvb, offCur + 1);
	    psn = 0;
	    cbHeader = 3;
	    break;

	case ACK:
	    cbHeader = 3;
	    break;

	case NEGATIVE_ACK:
	    /* Variable number of missing packets */
	    numMissing = tvb_get_guint8(tvb, offCur + 3);
	    cbHeader = numMissing + 4;
	    break;

	default:
	    break;
    };
    if (fRID) {
	strcat( szInfo, " R" );
    };
    if (fCon) {				/* Scan variable part (TPI's),	*/
					/* determine length of it	*/
	unsigned char	tCon;
	unsigned char	tByte;

	do {
	    tByte = tvb_get_guint8(tvb, offCur + cbHeader + vHeader);
	    tCon = tByte & 0x80;
	    if (tByte & 0x04)	/* Long format	*/
		vHeader = vHeader + tvb_get_guint8(tvb,
					offCur + cbHeader + vHeader + 1) + 2;
	    else
		vHeader = vHeader + (tByte & 0x03) + 1;
	} while (tCon);
    }

#ifdef DEBUG
    fprintf( stderr, "dissect_wtp: cbHeader = %d\n", cbHeader );
#endif

    /* Only update "Info" column when no data in this PDU will
     * be handed off to a subsequent dissector.
     */
    if (check_col(pinfo->cinfo, COL_INFO) &&
	((tvb_length_remaining(tvb, offCur + cbHeader + vHeader) <= 0) ||
	 (pdut == ACK) || (pdut==NEGATIVE_ACK) || (pdut==ABORT)) ) {
#ifdef DEBUG
	fprintf(stderr, "dissect_wtp: (6) About to set info_col header to %s\n", szInfo);
#endif
	col_append_str(pinfo->cinfo, COL_INFO, szInfo);
    };
    /* In the interest of speed, if "tree" is NULL, don't do any work not
       necessary to generate protocol tree items. */
    if (tree) {
#ifdef DEBUG
	fprintf(stderr, "dissect_wtp: cbHeader = %d\n", cbHeader);
#endif
	ti = proto_tree_add_item(tree, proto_wtp, tvb, offCur, cbHeader + vHeader, bo_little_endian);
#ifdef DEBUG
	fprintf(stderr, "dissect_wtp: (7) Returned from proto_tree_add_item\n");
#endif
	wtp_tree = proto_item_add_subtree(ti, ett_wtp);

/* Code to process the packet goes here */
#ifdef DEBUG
	fprintf(stderr, "dissect_wtp: cbHeader = %d\n", cbHeader);
	fprintf(stderr, "dissect_wtp: offCur = %d\n", offCur);
#endif
	/* Add common items: only CON and PDU Type */
	proto_tree_add_item(
			wtp_tree,	 		/* tree */
			hf_wtp_header_flag_continue, 	/* id */
			tvb,
			offCur, 			/* start of highlight */
			1,				/* length of highlight*/
			b0				/* value */
	     );
	proto_tree_add_item(wtp_tree, hf_wtp_header_pdu_type, tvb, offCur, 1, bo_little_endian);

	switch(pdut) {
	    case INVOKE:
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_Trailer, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_RID, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);

		proto_tree_add_item(wtp_tree, hf_wtp_header_Inv_version , tvb, offCur + 3, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_Inv_flag_TIDNew, tvb, offCur + 3, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_Inv_flag_UP, tvb, offCur + 3, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_Inv_Reserved, tvb, offCur + 3, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_Inv_TransactionClass, tvb, offCur + 3, 1, bo_little_endian);
		break;

	    case RESULT:
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_Trailer, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_RID, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);
		break;

	    case ACK:
		proto_tree_add_item(wtp_tree, hf_wtp_header_Ack_flag_TVETOK, tvb, offCur, 1, bo_big_endian);

		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_RID, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);
		break;

	    case ABORT:
		abortType = tvb_get_guint8 (tvb, offCur) & 0x07;
		proto_tree_add_item(wtp_tree, hf_wtp_header_Abort_type , tvb, offCur , 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);

		if (abortType == PROVIDER)
		{
		    proto_tree_add_item( wtp_tree, hf_wtp_header_Abort_reason_provider , tvb, offCur + 3 , 1, bo_little_endian);
		}
		else if (abortType == USER)
		{
		    proto_tree_add_item(wtp_tree, hf_wtp_header_Abort_reason_user , tvb, offCur + 3 , 1, bo_little_endian);
		}
		break;

	    case SEGMENTED_INVOKE:
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_Trailer, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_RID, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);

		proto_tree_add_item(wtp_tree, hf_wtp_header_sequence_number , tvb, offCur + 3, 1, bo_little_endian);
		break;

	    case SEGMENTED_RESULT:
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_Trailer, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_RID, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);

		proto_tree_add_item(wtp_tree, hf_wtp_header_sequence_number , tvb, offCur + 3, 1, bo_little_endian);
		break;

	    case NEGATIVE_ACK:
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_RID, tvb, offCur, 1, bo_little_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID_response, tvb, offCur + 1, 2, bo_big_endian);
		proto_tree_add_item(wtp_tree, hf_wtp_header_flag_TID, tvb, offCur + 1, 2, bo_big_endian);

		proto_tree_add_item(wtp_tree, hf_wtp_header_missing_packets , tvb, offCur + 3, 1, bo_little_endian);
		/* Iterate through missing packets */
		for (i = 0; i < numMissing; i++)
		{
		    proto_tree_add_item(wtp_tree, hf_wtp_header_sequence_number, tvb, offCur + 4 + i, 1, bo_little_endian);
		}
		break;

	    default:
		break;
	};
	if (fCon) {			/* Now, analyze variable part	*/
	    unsigned char	 tCon;
	    unsigned char	 tByte;
	    unsigned char	 tpiLen;
	    tvbuff_t		*tmp_tvb;

	    vHeader = 0;		/* Start scan all over	*/

	    do {
		tByte = tvb_get_guint8(tvb, offCur + cbHeader + vHeader);
		tCon = tByte & 0x80;
		if (tByte & 0x04)	/* Long TPI	*/
		    tpiLen = 2 + tvb_get_guint8(tvb,
					    offCur + cbHeader + vHeader + 1);
		else
		    tpiLen = 1 + (tByte & 0x03);
		tmp_tvb = tvb_new_subset(tvb, offCur + cbHeader + vHeader,
					tpiLen, tpiLen);
		wtp_handle_tpi(wtp_tree, tmp_tvb);
		vHeader += tpiLen;
	    } while (tCon);
	} else {
		/* There is no variable part */
	}	/* End of variable part of header */
    } else {
#ifdef DEBUG
	fprintf(stderr, "dissect_wtp: (4) tree was %p\n", tree);
#endif
    }
    /*
     * Any remaining data ought to be WSP data (if not WTP ACK, NACK
     * or ABORT pdu), so hand off (defragmented) to the WSP dissector
     */
    if ((tvb_reported_length_remaining(tvb, offCur + cbHeader + vHeader) > 0) &&
	! ((pdut==ACK) || (pdut==NEGATIVE_ACK) || (pdut==ABORT)))
    {
	int	dataOffset = offCur + cbHeader + vHeader;
	gint    dataLen = tvb_reported_length_remaining(tvb, dataOffset);
	gboolean save_fragmented;

	if (((pdut == SEGMENTED_INVOKE) || (pdut == SEGMENTED_RESULT) ||
	    (((pdut == INVOKE) || (pdut == RESULT)) && (!fTTR))) &&
	    tvb_bytes_exist(tvb, dataOffset, dataLen))
	{					/* 1st part of segment	*/
	    save_fragmented = pinfo->fragmented;
	    pinfo->fragmented = TRUE;
	    fd_head = fragment_add_seq(tvb, dataOffset, pinfo, TID,
			    wtp_fragment_table, psn, dataLen, !fTTR);
	    if (fd_head != NULL)		/* Reassembled	*/
	    {
		wsp_tvb = tvb_new_real_data(fd_head->data,
					    fd_head->len,
					    fd_head->len);
		tvb_set_child_real_data_tvbuff(tvb, wsp_tvb);
		add_new_data_source(pinfo, wsp_tvb,
					"Reassembled WTP");
		pinfo->fragmented = FALSE;

		/* show all fragments */
		show_fragment_seq_tree(fd_head, &wtp_frag_items,
					wtp_tree, pinfo, wsp_tvb);

		call_dissector(wsp_handle, wsp_tvb, pinfo, tree);
	    }
	    else
	    {
		if (check_col(pinfo->cinfo, COL_INFO))		/* Won't call WSP so display */
		    col_append_str(pinfo->cinfo, COL_INFO, szInfo);
	    }
	    pinfo->fragmented = save_fragmented;
	}
	else
	{
	    /*
	     * Normal packet, or not all the fragment data is available;
	     * call next dissector.
	     */
	    wsp_tvb = tvb_new_subset(tvb, dataOffset, -1, -1);
	    call_dissector(wsp_handle, wsp_tvb, pinfo, tree);
	}
    }
}

/*
 * Called directly from UDP.
 * Put "WTP+WSP" into the "Protocol" column.
 */
static void
dissect_wtp_fromudp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    if (check_col(pinfo->cinfo, COL_PROTOCOL))
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "WTP+WSP" );
    if (check_col(pinfo->cinfo, COL_INFO))
	col_clear(pinfo->cinfo, COL_INFO);

    dissect_wtp_common(tvb, pinfo, tree);
}

/*
 * Called from a higher-level WAP dissector, presumably WTLS.
 * Put "WTLS+WSP+WTP" to the "Protocol" column.
 *
 * XXX - is this supposed to be called from WTLS?  If so, we're not
 * calling it....
 *
 * XXX - can this be called from any other dissector?
 */
static void
dissect_wtp_fromwap(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    if (check_col(pinfo->cinfo, COL_PROTOCOL))
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "WTLS+WTP+WSP" );
    if (check_col(pinfo->cinfo, COL_INFO))
	col_clear(pinfo->cinfo, COL_INFO);

    dissect_wtp_common(tvb, pinfo, tree);
}

/* Register the protocol with Ethereal */
void
proto_register_wtp(void)
{

    /* Setup list of header fields */
    static hf_register_info hf[] = {
	{ &hf_wtp_header_sub_pdu_size,
	    { 	"Sub PDU size",
		"wtp.sub_pdu_size",
		FT_BYTES, BASE_HEX, NULL, 0x0,
		"Size of Sub-PDU", HFILL
	    }
	},
	{ &hf_wtp_header_flag_continue,
	    { 	"Continue Flag",
		"wtp.continue_flag",
		FT_BOOLEAN, 8, TFS( &continue_truth ), 0x80,
		"Continue Flag", HFILL
	    }
	},
	{ &hf_wtp_header_pdu_type,
	    { 	"PDU Type",
		"wtp.pdu_type",
		FT_UINT8, BASE_HEX, VALS( vals_pdu_type ), 0x78,
		"PDU Type", HFILL
	    }
	},
	{ &hf_wtp_header_flag_Trailer,
	    { 	"Trailer Flags",
		"wtp.trailer_flags",
		FT_UINT8, BASE_HEX, VALS( vals_transaction_trailer ), 0x06,
		"Trailer Flags", HFILL
	    }
	},
	{ &hf_wtp_header_flag_RID,
	    { 	"Re-transmission Indicator",
		"wtp.RID",
		FT_BOOLEAN, 8, TFS( &RID_truth ), 0x01,
		"Re-transmission Indicator", HFILL
	    }
	},
	{ &hf_wtp_header_flag_TID_response,
	    { 	"TID Response",
		"wtp.TID.response",
		FT_BOOLEAN, 16, TFS( &tid_response_truth ), 0x8000,
		"TID Response", HFILL
	    }
	},
	{ &hf_wtp_header_flag_TID,
	    { 	"Transaction ID",
		"wtp.TID",
		FT_UINT16, BASE_HEX, NULL, 0x7FFF,
		"Transaction ID", HFILL
	    }
	},
	{ &hf_wtp_header_Inv_version,
	    { 	"Version",
		"wtp.header.version",
		FT_UINT8, BASE_HEX, VALS( vals_version ), 0xC0,
		"Version", HFILL
	    }
	},
	{ &hf_wtp_header_Inv_flag_TIDNew,
	    { 	"TIDNew",
		"wtp.header.TIDNew",
		FT_BOOLEAN, 8, TFS( &TIDNew_truth ), 0x20,
		"TIDNew", HFILL
	    }
	},
	{ &hf_wtp_header_Inv_flag_UP,
	    { 	"U/P flag",
		"wtp.header.UP",
		FT_BOOLEAN, 8, TFS( &UP_truth ), 0x10,
		"U/P Flag", HFILL
	    }
	},
	{ &hf_wtp_header_Inv_Reserved,
	    { 	"Reserved",
		"wtp.inv.reserved",
		FT_UINT8, BASE_HEX, NULL, 0x0C,
		"Reserved", HFILL
	    }
	},
	{ &hf_wtp_header_Inv_TransactionClass,
	    { 	"Transaction Class",
		"wtp.inv.transaction_class",
		FT_UINT8, BASE_HEX, VALS( vals_transaction_classes ), 0x03,
		"Transaction Class", HFILL
	    }
	},
	{ &hf_wtp_header_Ack_flag_TVETOK,
	    { 	"Tve/Tok flag",
		"wtp.ack.tvetok",
		FT_BOOLEAN, 8, TFS( &TVETOK_truth ), 0x04,
		"Tve/Tok flag", HFILL
	    }
	},
	{ &hf_wtp_header_Abort_type,
	    { 	"Abort Type",
		"wtp.abort.type",
		FT_UINT8, BASE_HEX, VALS ( vals_abort_type ), 0x07,
		"Abort Type", HFILL
	    }
	},
	{ &hf_wtp_header_Abort_reason_provider,
	    { 	"Abort Reason",
		"wtp.abort.reason.provider",
		FT_UINT8, BASE_HEX, VALS ( vals_abort_reason_provider ), 0x00,
		"Abort Reason", HFILL
	    }
	},
	/* Assume WSP is the user and use its reason codes */
	{ &hf_wtp_header_Abort_reason_user,
	    { 	"Abort Reason",
		"wtp.abort.reason.user",
		FT_UINT8, BASE_HEX, VALS ( vals_wsp_reason_codes ), 0x00,
		"Abort Reason", HFILL
	    }
	},
	{ &hf_wtp_header_sequence_number,
	    { 	"Packet Sequence Number",
		"wtp.header.sequence",
		FT_UINT8, BASE_DEC, NULL, 0x00,
		"Packet Sequence Number", HFILL
	    }
	},
	{ &hf_wtp_header_missing_packets,
	    { 	"Missing Packets",
		"wtp.header.missing_packets",
		FT_UINT8, BASE_DEC, NULL, 0x00,
		"Missing Packets", HFILL
	    }
	},
	{ &hf_wtp_header_variable_part,
	    { 	"Header: Variable part",
		"wtp.header_variable_part",
		FT_BYTES, BASE_HEX, NULL, 0x0,
		"Variable part of the header", HFILL
	    }
	},
	{ &hf_wtp_data,
	    { 	"Data",
		"wtp.header_data",
		FT_BYTES, BASE_HEX, NULL, 0x0,
		"Data", HFILL
	    }
	},
	{ &hf_wtp_tpi_type,
	    { 	"TPI",
		"wtp.tpi",
		FT_UINT8, BASE_HEX, VALS(vals_tpi_type), 0x00,
		"Identification of the Transport Information Item", HFILL
	    }
	},
	{ &hf_wtp_tpi_psn,
	    { 	"Packet sequence number",
		"wtp.tpi.psn",
		FT_UINT8, BASE_DEC, NULL, 0x00,
		"Sequence number of this packet", HFILL
	    }
	},
	{ &hf_wtp_tpi_opt,
	    { 	"Option",
		"wtp.tpi.opt",
		FT_UINT8, BASE_HEX, VALS(vals_tpi_opt), 0x00,
		"The given option for this TPI", HFILL
	    }
	},
	{ &hf_wtp_tpi_optval,
	    { 	"Option Value",
		"wtp.tpi.opt.val",
		FT_NONE, BASE_NONE, NULL, 0x00,
		"The value that is supplied with this option", HFILL
	    }
	},
	{ &hf_wtp_tpi_info,
	    { 	"Information",
		"wtp.tpi.info",
		FT_NONE, BASE_NONE, NULL, 0x00,
		"The information being send by this TPI", HFILL
	    }
	},

	/* Fragment fields */
	{ &hf_wtp_fragment_overlap,
	    {	"Fragment overlap",
		"wtp.fragment.overlap",
		FT_BOOLEAN, BASE_NONE, NULL, 0x0,
		"Fragment overlaps with other fragments", HFILL
	    }
	},
	{ &hf_wtp_fragment_overlap_conflict,
	    {	"Conflicting data in fragment overlap",
		"wtp.fragment.overlap.conflict",
		FT_BOOLEAN, BASE_NONE, NULL, 0x0,
		"Overlapping fragments contained conflicting data", HFILL
	    }
	},
	{ &hf_wtp_fragment_multiple_tails,
	    {	"Multiple tail fragments found",
		"wtp.fragment.multipletails",
		FT_BOOLEAN, BASE_NONE, NULL, 0x0,
		"Several tails were found when defragmenting the packet", HFILL
	    }
	},
	{ &hf_wtp_fragment_too_long_fragment,
	    {	"Fragment too long",
		"wtp.fragment.toolongfragment",
		FT_BOOLEAN, BASE_NONE, NULL, 0x0,
		"Fragment contained data past end of packet", HFILL
	    }
	},
	{ &hf_wtp_fragment_error,
	    {	"Defragmentation error",
		"wtp.fragment.error",
		FT_FRAMENUM, BASE_NONE, NULL, 0x0,
		"Defragmentation error due to illegal fragments", HFILL
	    }
	},
	{ &hf_wtp_fragment,
	    {	"WTP Fragment",
		"wtp.fragment",
		FT_FRAMENUM, BASE_NONE, NULL, 0x0,
		"WTP Fragment", HFILL
	    }
	},
	{ &hf_wtp_fragments,
	    {	"WTP Fragments",
		"wtp.fragments",
		FT_NONE, BASE_NONE, NULL, 0x0,
		"WTP Fragments", HFILL
	    }
	},
    };

    /* Setup protocol subtree array */
    static gint *ett[] = {
	&ett_wtp,
	&ett_header,
	&ett_tpilist,
	&ett_wsp_fragments,
	&ett_wtp_fragment,
    };

    /* Register the protocol name and description */
    proto_wtp = proto_register_protocol(
	"Wireless Transaction Protocol",   /* protocol name for use by ethereal */
	"WTP",                             /* short version of name */
	"wap-wsp-wtp"                      /* Abbreviated protocol name, should Match IANA
					    < URL:http://www.isi.edu/in-notes/iana/assignments/port-numbers/ >
					    */
    );

    /* Required calls to register the header fields and subtrees used */
    proto_register_field_array(proto_wtp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    register_dissector("wtp", dissect_wtp_fromwap, proto_wtp);
    register_dissector("wtp-udp", dissect_wtp_fromudp, proto_wtp);
    register_init_routine(wtp_defragment_init);
};

void
proto_reg_handoff_wtp(void)
{
    dissector_handle_t wtp_fromudp_handle;

    /*
     * Get a handle for the connection-oriented WSP dissector - if WTP
     * PDUs have data, it is WSP.
     */
    wsp_handle = find_dissector("wsp-co");

    wtp_fromudp_handle = find_dissector("wtp-udp");
    dissector_add("udp.port", UDP_PORT_WTP_WSP, wtp_fromudp_handle);
}
