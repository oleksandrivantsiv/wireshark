/* packet-ndps.c
 * Routines for NetWare's NDPS
 * Greg Morris <gmorris@novell.com>
 * Copyright (c) Novell, Inc. 2002-2003
 *
 * $Id: packet-ndps.c,v 1.20 2003/04/20 11:36:14 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
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

#include <string.h>
#include <glib.h>
#include <epan/packet.h>
#include "prefs.h"
#include "packet-ipx.h"
#include "packet-tcp.h"
#include <epan/conversation.h>
#include "packet-ndps.h"
#include "reassemble.h"

#define NDPS_PACKET_INIT_COUNT	200

/* Tables for reassembly of fragments. */
static GHashTable *ndps_fragment_table = NULL;
static GHashTable *ndps_reassembled_table = NULL;

/* desegmentation of ndps */
static gboolean ndps_defragment = TRUE;
static guint32  frag_number = 0;
static guint32  save_frag_length=0;
static guint32  save_frag_seq=0;
static gboolean ndps_fragmented = FALSE;
static gboolean more_fragment = FALSE;
static guint32  tid = 1;

static void dissect_ndps_request(tvbuff_t*, packet_info*, proto_tree*, guint32, guint32, int);

static void dissect_ndps_reply(tvbuff_t *, packet_info*, proto_tree*, int);

static int hf_ndps_segments = -1;
static int hf_ndps_segment = -1;
static int hf_ndps_segment_overlap = -1;
static int hf_ndps_segment_overlap_conflict = -1;
static int hf_ndps_segment_multiple_tails = -1;
static int hf_ndps_segment_too_long_segment = -1;
static int hf_ndps_segment_error = -1;

static gint ett_ndps_segments = -1;
static gint ett_ndps_segment = -1;

static int proto_ndps = -1;
static int hf_ndps_record_mark = -1;
static int hf_ndps_length = -1;
static int hf_ndps_xid = -1;
static int hf_ndps_packet_type = -1;
static int hf_ndps_rpc_version = -1;
static int hf_ndps_error = -1;
static int hf_ndps_items = -1;
static int hf_ndps_objects = -1;
static int hf_ndps_attributes = -1;
static int hf_ndps_sbuffer = -1;
static int hf_ndps_rbuffer = -1;
static int hf_ndps_user_name = -1;
static int hf_ndps_broker_name = -1;
static int hf_ndps_pa_name = -1;
static int hf_ndps_tree = -1;
static int hf_ndps_reqframe = -1;
static int hf_ndps_error_val = -1;
static int hf_ndps_ext_error = -1;
static int hf_ndps_object = -1;
static int hf_ndps_cred_type = -1;
static int hf_ndps_server_name = -1;
static int hf_ndps_connection = -1;
static int hf_ndps_auth_null = -1;
static int hf_ndps_rpc_accept = -1;
static int hf_ndps_rpc_acc_stat = -1;
static int hf_ndps_rpc_rej_stat = -1;
static int hf_ndps_rpc_acc_results = -1;
static int hf_ndps_problem_type = -1;
static int hf_security_problem_type = -1;
static int hf_service_problem_type = -1;
static int hf_access_problem_type = -1;
static int hf_printer_problem_type = -1;
static int hf_selection_problem_type = -1;
static int hf_doc_access_problem_type = -1;
static int hf_attribute_problem_type = -1;
static int hf_update_problem_type = -1;
static int hf_obj_id_type = -1;
static int hf_oid_struct_size = -1;
static int hf_object_name = -1;
static int hf_ndps_document_number = -1;
static int hf_ndps_nameorid = -1;
static int hf_local_object_name = -1;
static int hf_printer_name = -1;
static int hf_ndps_qualified_name = -1;
static int hf_ndps_item_count = -1;
static int hf_ndps_qualifier = -1;
static int hf_ndps_lib_error = -1;
static int hf_ndps_other_error = -1;
static int hf_ndps_other_error_2 = -1;
static int hf_ndps_session = -1;
static int hf_ndps_abort_flag = -1;
static int hf_obj_attribute_type = -1;
static int hf_ndps_attribute_value = -1;
static int hf_ndps_lower_range = -1;
static int hf_ndps_upper_range = -1;
static int hf_ndps_n64 = -1;
static int hf_ndps_lower_range_n64 = -1;
static int hf_ndps_upper_range_n64 = -1;
static int hf_ndps_attrib_boolean = -1;
static int hf_ndps_realization = -1;
static int hf_ndps_xdimension_n64 = -1;
static int hf_ndps_ydimension_n64 = -1;
static int hf_ndps_dim_value = -1;
static int hf_ndps_dim_flag = -1;
static int hf_ndps_xydim_value = -1;
static int hf_ndps_location_value = -1;
static int hf_ndps_xmin_n64 = -1;
static int hf_ndps_xmax_n64 = -1;
static int hf_ndps_ymin_n64 = -1;
static int hf_ndps_ymax_n64 = -1;
static int hf_ndps_edge_value = -1;
static int hf_ndps_cardinal_or_oid = -1;
static int hf_ndps_cardinal_name_or_oid = -1;
static int hf_ndps_integer_or_oid = -1;
static int hf_ndps_profile_id = -1;
static int hf_ndps_persistence = -1;
static int hf_ndps_language_id = -1;
static int hf_address_type = -1;
static int hf_ndps_address = -1;
static int hf_ndps_add_bytes = -1;
static int hf_ndps_event_type = -1;
static int hf_ndps_event_object_identifier = -1;
static int hf_ndps_octet_string = -1;
static int hf_ndps_scope = -1;
static int hf_address_len = -1;
static int hf_ndps_net = -1;
static int hf_ndps_node = -1;
static int hf_ndps_socket = -1;
static int hf_ndps_port = -1;
static int hf_ndps_ip = -1;
static int hf_ndps_server_type = -1;
static int hf_ndps_service_type = -1;
static int hf_ndps_service_enabled = -1;
static int hf_ndps_method_name = -1;
static int hf_ndps_method_ver = -1;
static int hf_ndps_file_name = -1;
static int hf_ndps_admin_submit = -1;
static int hf_ndps_oid = -1;
static int hf_ndps_object_op = -1;
static int hf_answer_time = -1;
static int hf_oid_asn1_type = -1;
static int hf_ndps_item_ptr = -1;
static int hf_ndps_len = -1;
static int hf_limit_enc = -1;
static int hf_ndps_qualified_name2 = -1;
static int hf_ndps_delivery_add_type = -1;
static int hf_ndps_criterion_type = -1;
static int hf_ndps_ignored_type = -1;
static int hf_ndps_resource_type = -1;
static int hf_ndps_identifier_type = -1;
static int hf_ndps_page_flag = -1;
static int hf_ndps_media_type = -1;
static int hf_ndps_doc_content = -1;
static int hf_ndps_page_size = -1;
static int hf_ndps_direction = -1;
static int hf_ndps_page_order = -1;
static int hf_ndps_medium_size = -1;
static int hf_ndps_long_edge_feeds = -1;
static int hf_ndps_inc_across_feed = -1;
static int hf_ndps_size_inc_in_feed = -1;
static int hf_ndps_page_orientation = -1;
static int hf_ndps_numbers_up = -1;
static int hf_ndps_xdimension = -1;
static int hf_ndps_ydimension = -1;
static int hf_ndps_state_severity = -1;
static int hf_ndps_training = -1;
static int hf_ndps_colorant_set = -1;
static int hf_ndps_card_enum_time = -1;
static int hf_ndps_attrs_arg = -1;
static int hf_ndps_context = -1;
static int hf_ndps_filter = -1;
static int hf_ndps_item_filter = -1;
static int hf_ndps_substring_match = -1;
static int hf_ndps_time_limit = -1;
static int hf_ndps_count_limit = -1;
static int hf_ndps_operator = -1;
static int hf_ndps_password = -1;
static int hf_ndps_retrieve_restrictions = -1;
static int hf_bind_security = -1;
static int hf_ndps_max_items = -1;
static int hf_ndps_status_flags = -1;
static int hf_ndps_resource_list_type = -1;
static int hf_os_type = -1;
static int hf_ndps_printer_type = -1;
static int hf_ndps_printer_manuf = -1;
static int hf_ndps_inf_file_name = -1;
static int hf_ndps_vendor_dir = -1;
static int hf_banner_type = -1;
static int hf_font_type = -1;
static int hf_printer_id = -1;
static int hf_ndps_font_name = -1;
static int hf_ndps_return_code = -1;
static int hf_ndps_banner_name = -1;
static int hf_font_type_name = -1;
static int hf_font_file_name = -1;
static int hf_ndps_prn_file_name = -1;
static int hf_ndps_prn_dir_name = -1;
static int hf_ndps_def_file_name = -1;
static int hf_ndps_win31_items = -1;
static int hf_ndps_win95_items = -1;
static int hf_ndps_windows_key = -1;
static int hf_archive_type = -1;
static int hf_archive_file_size = -1;
static int hf_ndps_data = -1;
static int hf_get_status_flag = -1;
static int hf_res_type = -1;
static int hf_file_timestamp = -1;
static int hf_sub_complete = -1;
static int hf_doc_content = -1;
static int hf_ndps_doc_name = -1;
static int hf_print_arg = -1;
static int hf_local_id = -1;
static int hf_ndps_included_doc = -1;
static int hf_ndps_ref_name = -1;
static int hf_interrupt_job_type = -1;
static int hf_pause_job_type = -1;
static int hf_ndps_force = -1;
static int hf_resubmit_op_type = -1;
static int hf_shutdown_type = -1;
static int hf_ndps_supplier_flag = -1;
static int hf_ndps_language_flag = -1;
static int hf_ndps_method_flag = -1;
static int hf_ndps_delivery_address_flag = -1;
static int hf_ndps_list_profiles_type = -1;
static int hf_ndps_list_profiles_choice_type = -1;
static int hf_ndps_list_profiles_result_type = -1;
static int hf_ndps_integer_type_flag = -1;
static int hf_ndps_integer_type_value = -1;
static int hf_ndps_continuation_option = -1;
static int hf_ndps_ds_info_type = -1;
static int hf_ndps_guid = -1;
static int hf_ndps_list_services_type = -1;
static int hf_ndps_item_bytes = -1;
static int hf_ndps_certified = -1;
static int hf_ndps_attribute_set = -1;
static int hf_ndps_data_item_type = -1;
static int hf_info_int = -1;
static int hf_info_int16 = -1;
static int hf_info_int32 = -1;
static int hf_info_boolean = -1;
static int hf_info_string = -1;
static int hf_info_bytes = -1;
static int hf_ndps_list_local_servers_type = -1;
static int hf_ndps_registry_name = -1;
static int hf_ndps_client_server_type = -1;
static int hf_ndps_session_type = -1;
static int hf_time = -1;
static int hf_ndps_supplier_name = -1;
static int hf_ndps_message = -1;
static int hf_delivery_method_type = -1;
static int hf_ndps_get_session_type = -1;
static int hf_packet_count = -1;
static int hf_last_packet_flag = -1;
static int hf_ndps_get_resman_session_type = -1;
static int hf_problem_type = -1;

static int hf_spx_ndps_program = -1;
static int hf_spx_ndps_version = -1;
static int hf_spx_ndps_func_print = -1;
static int hf_spx_ndps_func_registry = -1;
static int hf_spx_ndps_func_notify = -1;
static int hf_spx_ndps_func_resman = -1;
static int hf_spx_ndps_func_delivery = -1;
static int hf_spx_ndps_func_broker = -1;

static gint ett_ndps = -1;
static dissector_handle_t ndps_data_handle;

/* desegmentation of NDPS over TCP */
static gboolean ndps_desegment = TRUE;

static const value_string true_false[] = {
    { 0x00000000, "Accept" },
    { 0x00000001, "Deny" },
    { 0,          NULL }
};

static const value_string ndps_limit_enc_enum[] = {
    { 0x00000000, "Time" },
    { 0x00000001, "Count" },
    { 0x00000002, "Error" },
    { 0,          NULL }
};

static const value_string problem_type_enum[] = {
    { 0x00000000, "Standard" },
    { 0x00000001, "Extended" },
    { 0,          NULL }
};

static const value_string accept_stat[] = {
    { 0x00000000, "Success" },
    { 0x00000001, "Program Unavailable" },
    { 0x00000002, "Program Mismatch" },
    { 0x00000003, "Procedure Unavailable" },
    { 0x00000004, "Garbage Arguments" },
    { 0x00000005, "System Error" },
    { 0,          NULL }
};

static const value_string reject_stat[] = {
    { 0x00000000, "RPC Mismatch" },
    { 0x00000001, "Authentication Error" },
    { 0,          NULL }
};

static const value_string error_type_enum[] = {
    { 0x00000000, "Security Error" },
    { 0x00000001, "Service Error" },
    { 0x00000002, "Access Error" },
    { 0x00000003, "Printer Error" },
    { 0x00000004, "Selection Error" },
    { 0x00000005, "Document Access Error" },
    { 0x00000006, "Attribute Error" },
    { 0x00000007, "Update Error" },
    { 0,          NULL }
};

static const value_string security_problem_enum[] = {
    { 0x00000000, "Authentication" },
    { 0x00000001, "Credentials" },
    { 0x00000002, "Rights" },
    { 0x00000003, "Invalid PAC" },
    { 0,          NULL }
};

static const value_string service_problem_enum[] = {
    { 0x00000000, "Sever Busy" },
    { 0x00000001, "Server Unavailable" },
    { 0x00000002, "Complex Operation" },
    { 0x00000003, "Resource Limit" },
    { 0x00000004, "Unclassified Server Error" },
    { 0x00000005, "Too Many Items in List" },
    { 0x00000006, "Resource not Available" },
    { 0x00000007, "Cancel Document Support" },
    { 0x00000008, "Modify Document Support" },
    { 0x00000009, "Multiple Document Support" },
    { 0x0000000a, "Parameter Valid Support" },
    { 0x0000000b, "Invalid Checkpoint" },
    { 0x0000000c, "Continuation Context" },
    { 0x0000000d, "Pause Limit Exceeded" },
    { 0x0000000e, "Unsupported Operation" },
    { 0x0000000f, "Notify Service Error" },
    { 0x00000010, "Accounting Service Error" },
    { 0,          NULL }
};

static const value_string access_problem_enum[] = {
    { 0x00000000, "Wrong Object Class" },
    { 0x00000001, "Lack of Access Rights" },
    { 0x00000002, "Can't Interrupt Job" },
    { 0x00000003, "Wrong Object State" },
    { 0x00000004, "Client Not Bound" },
    { 0x00000005, "Not Available" },
    { 0x00000006, "Notify Service Not Connected" },
    { 0x00000007, "PDS Not Connected" },
    { 0,          NULL }
};

static const value_string printer_problem_enum[] = {
    { 0x00000000, "Printer Error" },
    { 0x00000001, "Printer Needs Attention" },
    { 0x00000002, "Printer Needs Key Operator" },
    { 0,          NULL }
};

static const value_string selection_problem_enum[] = {
    { 0x00000000, "Invalid ID" },
    { 0x00000001, "Unknown ID" },
    { 0x00000002, "Object Exists" },
    { 0x00000003, "ID Changed" },
    { 0,          NULL }
};

static const value_string doc_access_problem_enum[] = {
    { 0x00000000, "Access Not Available" },
    { 0x00000001, "Time Expired" },
    { 0x00000002, "Access Denied" },
    { 0x00000003, "Unknown Document" },
    { 0x00000004, "No Documents in Job" },
    { 0,          NULL }
};

static const value_string attribute_problem_enum[] = {
    { 0x00000000, "Invalid Syntax" },
    { 0x00000001, "Undefined Type" },
    { 0x00000002, "Wrong Matching" },
    { 0x00000003, "Constraint Violated" },
    { 0x00000004, "Unsupported Type" },
    { 0x00000005, "Illegal Modification" },
    { 0x00000006, "Consists With Other Attribute" },
    { 0x00000007, "Undefined Attribute Value" },
    { 0x00000008, "Unsupported Value" },
    { 0x00000009, "Invalid Noncompulsed Modification" },
    { 0x0000000a, "Per Job Inadmissible" },
    { 0x0000000b, "Not Multivalued" },
    { 0x0000000c, "Mandatory Omitted" },
    { 0x0000000d, "Illegal For Class" },
    { 0,          NULL }
};

static const value_string update_problem_enum[] = {
    { 0x00000000, "No Modifications Allowed" },
    { 0x00000001, "Insufficient Rights" },
    { 0x00000002, "Previous Operation Incomplete" },
    { 0x00000003, "Cancel Not Possible" },
    { 0,          NULL }
};

static const value_string obj_identification_enum[] = {
    { 0x00000000, "Printer Contained Object ID" },
    { 0x00000001, "Document Identifier" },
    { 0x00000002, "Object Identifier" },
    { 0x00000003, "Object Name" },
    { 0x00000004, "Name or Object ID" },
    { 0x00000005, "Simple Name" },
    { 0x00000006, "Printer Configuration Object ID" },
    { 0x00000007, "Qualified Name" },
    { 0x00000008, "Event Object ID" },
    { 0,          NULL }
};

static const value_string nameorid_enum[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Global" },
    { 0x00000002, "Local" },
    { 0,          NULL }
};

static const value_string qualified_name_enum[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Simple" },
    { 0x00000002, "NDS" },
    { 0,          NULL }
};

static const value_string qualified_name_enum2[] = {
    { 0x00000000, "NDS" },
    { 0,          NULL }
};

static const value_string spx_ndps_program_vals[] = {
    { 0x00060976, "Print Program" },
    { 0x00060977, "Broker Program" },
    { 0x00060978, "Registry Program" },
    { 0x00060979, "Notify Program" },
    { 0x0006097a, "Resource Manager Program" },
    { 0x0006097b, "Programmatic Delivery Program" },
    { 0,          NULL }
};

static const value_string spx_ndps_print_func_vals[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Bind PSM" },
    { 0x00000002, "Bind PA" },
    { 0x00000003, "Unbind" },
    { 0x00000004, "Print" },
    { 0x00000005, "Modify Job" },
    { 0x00000006, "Cancel Job" },
    { 0x00000007, "List Object Attributes" },
    { 0x00000008, "Promote Job" },
    { 0x00000009, "Interrupt" },
    { 0x0000000a, "Pause" },
    { 0x0000000b, "Resume" },
    { 0x0000000c, "Clean" },
    { 0x0000000d, "Create" },
    { 0x0000000e, "Delete" },
    { 0x0000000f, "Disable PA" },
    { 0x00000010, "Enable PA" },
    { 0x00000011, "Resubmit Jobs" },
    { 0x00000012, "Set" },
    { 0x00000013, "Shutdown PA" },
    { 0x00000014, "Startup PA" },
    { 0x00000015, "Reorder Job" },
    { 0x00000016, "Pause PA" },
    { 0x00000017, "Resume PA" },
    { 0x00000018, "Transfer Data" },
    { 0x00000019, "Device Control" },
    { 0x0000001a, "Add Event Profile" },
    { 0x0000001b, "Remove Event Profile" },
    { 0x0000001c, "Modify Event Profile" },
    { 0x0000001d, "List Event Profiles" },
    { 0x0000001e, "Shutdown PSM" },
    { 0x0000001f, "Cancel PSM Shutdown" },
    { 0x00000020, "Set Printer DS Information" },
    { 0x00000021, "Clean User Jobs" },
    { 0x00000022, "Map GUID to NDS Name" },
    { 0,          NULL }
};

static const value_string spx_ndps_notify_func_vals[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Notify Bind" },
    { 0x00000002, "Notify Unbind" },
    { 0x00000003, "Register Supplier" },
    { 0x00000004, "Deregister Supplier" },
    { 0x00000005, "Add Profile" },
    { 0x00000006, "Remove Profile" },
    { 0x00000007, "Modify Profile" },
    { 0x00000008, "List Profiles" },
    { 0x00000009, "Report Event" },
    { 0x0000000a, "List Supported Languages" },
    { 0x0000000b, "Report Notification" },
    { 0x0000000c, "Add Delivery Method" },
    { 0x0000000d, "Remove Delivery Method" },
    { 0x0000000e, "List Delivery Methods" },
    { 0x0000000f, "Get Delivery Method Information" },
    { 0x00000010, "Get Notify NDS Object Name" },
    { 0x00000011, "Get Notify Session Information" },
    { 0,          NULL }
};

static const value_string spx_ndps_deliver_func_vals[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Delivery Bind" },
    { 0x00000002, "Delivery Unbind" },
    { 0x00000003, "Delivery Send" },
    { 0x00000004, "Delivery Send2" },
    { 0,          NULL }
};

static const value_string spx_ndps_registry_func_vals[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Bind" },
    { 0x00000002, "Unbind" },
    { 0x00000003, "Register Server" },
    { 0x00000004, "Deregister Server" },
    { 0x00000005, "Register Registry" },
    { 0x00000006, "Deregister Registry" },
    { 0x00000007, "Registry Update" },
    { 0x00000008, "List Local Servers" },
    { 0x00000009, "List Servers" },
    { 0x0000000a, "List Known Registries" },
    { 0x0000000b, "Get Registry NDS Object Name" },
    { 0x0000000c, "Get Registry Session Information" },
    { 0,          NULL }
};

static const value_string spx_ndps_resman_func_vals[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Bind" },
    { 0x00000002, "Unbind" },
    { 0x00000003, "Add Resource File" },
    { 0x00000004, "Delete Resource File" },
    { 0x00000005, "List Resources" },
    { 0x00000006, "Get Resource File" },
    { 0x00000007, "Get Resource File Date" },
    { 0x00000008, "Get Resource Manager NDS Object Name" },
    { 0x00000009, "Get Resource Manager Session Information" },
    { 0x0000000a, "Set Resource Language Context" },
    { 0,          NULL }
};

static const value_string spx_ndps_broker_func_vals[] = {
    { 0x00000000, "None" },
    { 0x00000001, "Bind" },
    { 0x00000002, "Unbind" },
    { 0x00000003, "List Services" },
    { 0x00000004, "Enable Service" },
    { 0x00000005, "Disable Service" },
    { 0x00000006, "Down Broker" },
    { 0x00000007, "Get Broker NDS Object Name" },
    { 0x00000008, "Get Broker Session Information" },
    { 0,          NULL }
};

static const value_string ndps_packet_types[] = {
    { 0x00000000, "Request" },
    { 0x00000001, "Reply" },
    { 0,          NULL }
};

static const value_string ndps_realization_enum[] = {
    { 0x00000000, "Logical" },
    { 0x00000001, "Physical" },
    { 0x00000002, "Logical & Physical" },
    { 0,          NULL }
};

static const value_string ndps_dim_value_enum[] = {
    { 0x00000000, "Numeric" },
    { 0x00000001, "Named" },
    { 0,          NULL }
};

static const value_string ndps_xydim_value_enum[] = {
    { 0x00000000, "Real" },
    { 0x00000001, "Named" },
    { 0x00000002, "Cardinal" },
    { 0,          NULL }
};

static const value_string ndps_location_value_enum[] = {
    { 0x00000000, "Numeric" },
    { 0x00000001, "Named" },
    { 0,          NULL }
};

static const value_string ndps_edge_value_enum[] = {
    { 0x00000000, "Bottom" },
    { 0x00000001, "Right" },
    { 0x00000002, "Top" },
    { 0x00000003, "Left" },
    { 0,          NULL }
};

static const value_string ndps_card_or_oid_enum[] = {
    { 0x00000000, "Number" },
    { 0x00000001, "ID" },
    { 0,          NULL }
};

static const value_string ndps_card_name_or_oid_enum[] = {
    { 0x00000000, "Number" },
    { 0x00000001, "ID" },
    { 0,          NULL }
};

static const value_string ndps_integer_or_oid_enum[] = {
    { 0x00000000, "ID" },
    { 0x00000001, "Number" },
    { 0,          NULL }
};

static const value_string ndps_persistence_enum[] = {
    { 0x00000000, "Permanent" },
    { 0x00000001, "Volatile" },
    { 0,          NULL }
};

static const value_string ndps_address_type_enum[] = {
    { 0x00000000, "User" },
    { 0x00000001, "Server" },
    { 0x00000002, "Volume" },
    { 0x00000003, "Organization Unit" },
    { 0x00000004, "Organization" },
    { 0x00000005, "Group" },
    { 0x00000006, "Distinguished Name" },
    { 0x00000007, "User or Container" },
    { 0x00000008, "Case Exact String" },
    { 0x00000009, "Case Ignore String" },
    { 0x0000000a, "Numeric String" },
    { 0x0000000b, "DOS File Name" },
    { 0x0000000c, "Phone Number" },
    { 0x0000000d, "Boolean" },
    { 0x0000000e, "Integer" },
    { 0x0000000f, "Network Address" },
    { 0x00000010, "Choice" },
    { 0x00000011, "GroupWise User" },
    { 0,          NULL }
};

static const value_string ndps_address_enum[] = {
    { 0x00000000, "IPX" },
    { 0x00000001, "IP" },
    { 0x00000002, "SDLC" },
    { 0x00000003, "Token Ring to Ethernet" },
    { 0x00000004, "OSI" },
    { 0x00000005, "AppleTalk" },
    { 0x00000006, "Count" },
    { 0,          NULL }
};


static const value_string ndps_server_type_enum[] = {
    { 0x00000000, "All" },
    { 0x00000001, "Public Access Printer Agent" },
    { 0x00000002, "Notification Server" },
    { 0x00000003, "Resource Manager" },
    { 0x00000004, "Network Port Handler" },
    { 0,          NULL }
};

static const value_string ndps_event_object_enum[] = {
    { 0x00000000, "Object" },
    { 0x00000001, "Filter" },
    { 0x00000002, "Detail" },
    { 0,          NULL }
};

static const value_string ndps_service_type_enum[] = {
    { 0x00000000, "SRS" },
    { 0x00000001, "ENS" },
    { 0x00000002, "RMS" },
    { 0,          NULL }
};

static const value_string ndps_delivery_add_enum[] = {
    { 0x00000000, "MHS Address" },
    { 0x00000001, "Distinguished Name" },
    { 0x00000002, "Text" },
    { 0x00000003, "Octet String" },
    { 0x00000004, "Distinguished Name String" },
    { 0x00000005, "RPC Address" },
    { 0x00000006, "Qualified Name" },
    { 0,          NULL }
};

static const value_string ndps_resource_enum[] = {
    { 0x00000000, "Name or ID" },
    { 0x00000001, "Text" },
    { 0,          NULL }
};


static const value_string ndps_identifier_enum[] = {
    { 0x00000000, "ID Nominal Number" },
    { 0x00000001, "ID Alpha-numeric" },
    { 0x00000002, "ID Tag" },
    { 0,          NULL }
};

static const value_string ndps_media_enum[] = {
    { 0x00000000, "Select All Pages" },
    { 0x00000001, "Selected Pages" },
    { 0,          NULL }
};

static const value_string ndps_page_size_enum[] = {
    { 0x00000000, "ID" },
    { 0x00000001, "Dimensions" },
    { 0,          NULL }
};

static const value_string ndps_pres_direction_enum[] = {
    { 0x00000000, "Right to Bottom" },
    { 0x00000001, "Left to Bottom" },
    { 0x00000002, "Bidirectional to Bottom" },
    { 0x00000003, "Right to Top" },
    { 0x00000004, "Left to Top" },
    { 0x00000005, "Bidirectional to Top" },
    { 0x00000006, "Bottom to Right" },
    { 0x00000007, "Bottom to Left" },
    { 0x00000008, "Top to Left" },
    { 0x00000009, "Top to Right" },
    { 0,          NULL }
};

static const value_string ndps_page_order_enum[] = {
    { 0x00000000, "Unknown" },
    { 0x00000001, "First to Last" },
    { 0x00000002, "Last to First" },
    { 0,          NULL }
};

static const value_string ndps_medium_size_enum[] = {
    { 0x00000000, "Discrete" },
    { 0x00000001, "Continuous" },
    { 0,          NULL }
};

static const value_string ndps_page_orientation_enum[] = {
    { 0x00000000, "Unknown" },
    { 0x00000001, "Face Up" },
    { 0x00000002, "Face Down" },
    { 0,          NULL }
};

static const value_string ndps_numbers_up_enum[] = {
    { 0x00000000, "Cardinal" },
    { 0x00000001, "Name or Object ID" },
    { 0x00000002, "Cardinal Range" },
    { 0,          NULL }
};


static const value_string ndps_state_severity_enum[] = {
    { 0x00000001, "Other" },
    { 0x00000002, "Warning" },
    { 0x00000003, "Critical" },
    { 0,          NULL }
};


static const value_string ndps_training_enum[] = {
    { 0x00000001, "Other" },
    { 0x00000002, "Unknown" },
    { 0x00000003, "Untrained" },
    { 0x00000004, "Trained" },
    { 0x00000005, "Field Service" },
    { 0x00000006, "Management" },
    { 0,          NULL }
};

static const value_string ndps_colorant_set_enum[] = {
    { 0x00000000, "Name" },
    { 0x00000001, "Description" },
    { 0,          NULL }
};

static const value_string ndps_card_enum_time_enum[] = {
    { 0x00000000, "Cardinal" },
    { 0x00000001, "Enumeration" },
    { 0x00000002, "Time" },
    { 0,          NULL }
};

static const value_string ndps_attrs_arg_enum[] = {
    { 0x00000000, "Continuation" },
    { 0x00000001, "Specification" },
    { 0,          NULL }
};


static const value_string ndps_filter_enum[] = {
    { 0x00000000, "Item" },
    { 0x00000001, "And" },
    { 0x00000002, "Or" },
    { 0x00000003, "Not" },
    { 0,          NULL }
};


static const value_string ndps_filter_item_enum[] = {
    { 0x00000000, "Equality" },
    { 0x00000001, "Substrings" },
    { 0x00000002, "Greater then or Equal to" },
    { 0x00000003, "Less then or Equal to" },
    { 0x00000004, "Present" },
    { 0x00000005, "Subset of" },
    { 0x00000006, "Superset of" },
    { 0x00000007, "Non NULL Set Intersect" },
    { 0,          NULL }
};

static const value_string ndps_match_criteria_enum[] = {
    { 0x00000000, "Exact" },
    { 0x00000001, "Case Insensitive" },
    { 0x00000002, "Same Letter" },
    { 0x00000003, "Approximate" },
    { 0,          NULL }
};

static const value_string ndps_operator_enum[] = {
    { 0x00000000, "Attributes" },
    { 0x00000002, "Ordered Jobs" },
    { 0,          NULL }
};

static const value_string ndps_resource_type_enum[] = {
    { 0x00000000, "Printer Drivers" },
    { 0x00000001, "Printer Definitions" },
    { 0x00000002, "Printer Definitions Short" },
    { 0x00000003, "Banner Page Files" },
    { 0x00000004, "Font Types" },
    { 0x00000005, "Printer Driver Files" },
    { 0x00000006, "Printer Definition File" },
    { 0x00000007, "Font Files" },
    { 0x00000008, "Generic Type" },
    { 0x00000009, "Generic Files" },
    { 0x0000000a, "Printer Definition File 2" },
    { 0x0000000b, "Printer Driver Types 2" },
    { 0x0000000c, "Printer Driver Files 2" },
    { 0x0000000d, "Printer Driver Types Archive" },
    { 0x0000000e, "Languages Available" },
    { 0,          NULL }
};

static const value_string ndps_os_type_enum[] = {
    { 0x00000000, "DOS" },
    { 0x00000001, "Windows 3.1" },
    { 0x00000002, "Windows 95" },
    { 0x00000003, "Windows NT" },
    { 0x00000004, "OS2" },
    { 0x00000005, "MAC" },
    { 0x00000006, "UNIX" },
    { 0x00000007, "Windows NT 4.0" },
    { 0x00000008, "Windows 2000/XP" },
    { 0x00000009, "Windows 98" },
    { 0xffffffff, "None" },
    { 0,          NULL }
};

static const value_string ndps_banner_type_enum[] = {
    { 0x00000000, "All" },
    { 0x00000001, "PCL" },
    { 0x00000002, "PostScript" },
    { 0x00000003, "ASCII Text" },
    { 0,          NULL }
};

static const value_string ndps_font_type_enum[] = {
    { 0x00000000, "TrueType" },
    { 0x00000001, "PostScript" },
    { 0x00000002, "System" },
    { 0x00000003, "SPD" },
    { 0x00000004, "True Doc" },
    { 0,          NULL }
};

static const value_string ndps_archive_enum[] = {
    { 0x00000000, "ZIP" },
    { 0x00000001, "JAR" },
    { 0,          NULL }
};


static const value_string ndps_res_type_enum[] = {
    { 0x00000000, "Printer Driver" },
    { 0x00000001, "Printer Definition" },
    { 0x00000002, "Banner Page" },
    { 0x00000003, "Font" },
    { 0x00000004, "Generic Resource" },
    { 0x00000005, "Print Driver Archive" },
    { 0,          NULL }
};

static const value_string ndps_print_arg_enum[] = {
    { 0x00000000, "Create Job" },
    { 0x00000001, "Add Document" },
    { 0x00000002, "Close Job" },
    { 0,          NULL }
};

static const value_string ndps_doc_content_enum[] = {
    { 0x00000000, "Content Included" },
    { 0x00000001, "Content Referenced" },
    { 0,          NULL }
};

static const value_string ndps_interrupt_job_enum[] = {
    { 0x00000000, "Job ID" },
    { 0x00000001, "Name" },
    { 0,          NULL }
};

static const value_string ndps_pause_job_enum[] = {
    { 0x00000000, "Job ID" },
    { 0x00000001, "Name" },
    { 0,          NULL }
};

static const value_string ndps_resubmit_op_enum[] = {
    { 0x00000000, "Copy" },
    { 0x00000001, "Move" },
    { 0,          NULL }
};

static const value_string ndps_shutdown_enum[] = {
    { 0x00000000, "Do Current Jobs" },
    { 0x00000001, "Immediate" },
    { 0x00000002, "Do Pending Jobs" },
    { 0,          NULL }
};

static const value_string ndps_list_profiles_choice_enum[] = {
    { 0x00000000, "ID" },
    { 0x00000001, "Filter" },
    { 0,          NULL }
};

static const value_string ndps_list_profiles_result_enum[] = {
    { 0x00000000, "Complete" },
    { 0x00000001, "No Event Objects" },
    { 0x00000002, "Profile ID's" },
    { 0,          NULL }
};

static const value_string ndps_ds_info_enum[] = {
    { 0x00000000, "Add" },
    { 0x00000001, "Remove" },
    { 0x00000002, "Update" },
    { 0,          NULL }
};

static const value_string ndps_list_services_enum[] = {
    { 0x00000000, "Supported" },
    { 0x00000001, "Enabled" },
    { 0,          NULL }
};

static const value_string ndps_data_item_enum[] = {
    { 0x00000000, "Int8" },
    { 0x00000001, "Int16" },
    { 0x00000002, "Int32" },
    { 0x00000003, "Boolean" },
    { 0x00000004, "Character String" },
    { 0x00000005, "Byte String" },
    { 0,          NULL }
};

static const value_string ndps_list_local_servers_enum[] = {
    { 0x00000000, "Specification" },
    { 0x00000001, "Continuation" },
    { 0,          NULL }
};

static const value_string ndps_delivery_method_enum[] = {
    { 0x00000000, "Specification" },
    { 0x00000001, "Continuation" },
    { 0,          NULL }
};

static const value_string ndps_attribute_enum[] = {
    { 0x00000000, "Null" },
    { 0x00000001, "Text" },
    { 0x00000002, "Descriptive Name" },
    { 0x00000003, "Descriptor" },
    { 0x00000004, "Message" },
    { 0x00000005, "Error Message" },
    { 0x00000006, "Simple Name" },
    { 0x00000007, "Distinguished Name String" },
    { 0x00000008, "Distinguished Name Seq" },
    { 0x00000009, "Delta Time" },
    { 0x0000000a, "Time" },
    { 0x0000000b, "Integer" },
    { 0x0000000c, "Integer Seq" },
    { 0x0000000d, "Cardinal" },
    { 0x0000000e, "Cardinal Seq" },
    { 0x0000000f, "Positive Integer" },
    { 0x00000010, "Integer Range" },
    { 0x00000011, "Cardinal Range" },
    { 0x00000012, "Maximum Integer" },
    { 0x00000013, "Minimum Integer" },
    { 0x00000014, "Integer 64" },
    { 0x00000015, "Integer 64 Seq" },
    { 0x00000016, "Cardinal 64" },
    { 0x00000017, "Cardinal 64 Seq" },
    { 0x00000018, "Positive Integer 64" },
    { 0x00000019, "Integer 64 Range" },
    { 0x0000001a, "Cardinal 64 Range" },
    { 0x0000001b, "Maximum Integer 64" },
    { 0x0000001c, "Minimum Integer 64" },
    { 0x0000001d, "Real" },
    { 0x0000001e, "Real Seq" },
    { 0x0000001f, "Non-Negative Real" },
    { 0x00000020, "Real Range" },
    { 0x00000021, "Non-Negative Real Range" },
    { 0x00000022, "Boolean" },
    { 0x00000023, "Percent" },
    { 0x00000024, "Object Identifier" },
    { 0x00000025, "Object Identifier Seq" },
    { 0x00000026, "Name or OID" },
    { 0x00000027, "Name or OID Seq" },
    { 0x00000028, "Distinguished Name" },
    { 0x00000029, "Relative Distinguished Name Seq" },
    { 0x0000002a, "Realization" },
    { 0x0000002b, "Medium Dimensions" },
    { 0x0000002c, "Dimension" },
    { 0x0000002d, "XY Dimensions" },
    { 0x0000002e, "Locations" },
    { 0x0000002f, "Area" },
    { 0x00000030, "Area Seq" },
    { 0x00000031, "Edge" },
    { 0x00000032, "Font Reference" },
    { 0x00000033, "Cardinal or OID" },
    { 0x00000034, "OID Cardinal Map" },
    { 0x00000035, "Cardinal or Name or OID" },
    { 0x00000036, "Positive Integer or OID" },
    { 0x00000037, "Event Handling Profile" },
    { 0x00000038, "Octet String" },
    { 0x00000039, "Priority" },
    { 0x0000003a, "Locale" },
    { 0x0000003b, "Method Delivery Address" },
    { 0x0000003c, "Object Identification" },
    { 0x0000003d, "Results Profile" },
    { 0x0000003e, "Criteria" },
    { 0x0000003f, "Job Password" },
    { 0x00000040, "Job Level" },
    { 0x00000041, "Job Categories" },
    { 0x00000042, "Print Checkpoint" },
    { 0x00000043, "Ignored Attribute" },
    { 0x00000044, "Resource" },
    { 0x00000045, "Medium Substitution" },
    { 0x00000046, "Font Substitution" },
    { 0x00000047, "Resource Context Seq" },
    { 0x00000048, "Sides" },
    { 0x00000049, "Page Select Seq" },
    { 0x0000004a, "Page Media Select" },
    { 0x0000004b, "Document Content" },
    { 0x0000004c, "Page Size" },
    { 0x0000004d, "Presentation Direction" },
    { 0x0000004e, "Page Order" },
    { 0x0000004f, "File Reference" },
    { 0x00000050, "Medium Source Size" },
    { 0x00000051, "Input Tray Medium" },
    { 0x00000052, "Output Bins Chars" },
    { 0x00000053, "Page ID Type" },
    { 0x00000054, "Level Range" },
    { 0x00000055, "Category Set" },
    { 0x00000056, "Numbers Up Supported" },
    { 0x00000057, "Finishing" },
    { 0x00000058, "Print Contained Object ID" },
    { 0x00000059, "Print Config Object ID" },
    { 0x0000005a, "Typed Name" },
    { 0x0000005b, "Network Address" },
    { 0x0000005c, "XY Dimensions Value" },
    { 0x0000005d, "Name or OID Dimensions Map" },
    { 0x0000005e, "Printer State Reason" },
    { 0x0000005f, "Enumeration" },
    { 0x00000060, "Qualified Name" },
    { 0x00000061, "Qualified Name Set" },
    { 0x00000062, "Colorant Set" },
    { 0x00000063, "Resource Printer ID" },
    { 0x00000064, "Event Object ID" },
    { 0x00000065, "Qualified Name Map" },
    { 0x00000066, "File Path" },
    { 0x00000067, "Uniform Resource Identifier" },
    { 0x00000068, "Cardinal or Enum or Time" },
    { 0x00000069, "Print Contained Object ID Set" },
    { 0x0000006a, "Octet String Pair" },
    { 0x0000006b, "Octet String Integer Pair" },
    { 0x0000006c, "Extended Resource Identifier" },
    { 0x0000006d, "Event Handling Profile 2" },
    { 0,          NULL }
};

static const value_string ndps_error_types[] = {
    { 0x00000000, "Ok" },
    { 0x00000001, "Error" },
    { 0xFFFFFC18, "Broker Out of Memory" },      /* Broker Errors */
    { 0xFFFFFC17, "Broker Bad NetWare Version" },
    { 0xFFFFFC16, "Broker Wrong Command Line Arguments" },
    { 0xFFFFFC15, "Broker Name Not Given" },
    { 0xFFFFFC14, "Not Broker Class" },
    { 0xFFFFFC13, "Invalid Broker Password" },
    { 0xFFFFFC12, "Invalid Broker Name" },
    { 0xFFFFFC11, "Broker Failed to Create Thread" },
    { 0xFFFFFC10, "Broker Failed to Initialize NUT" },
    { 0xFFFFFC0F, "Broker Failed to Get Messages" },
    { 0xFFFFFC0E, "Broker Failed to Allocate Resources" },
    { 0xFFFFFC0D, "Broker Service Name Must be Fully Distinguished" },
    { 0xFFFFFC0C, "Broker Uninitialized Module" },
    { 0xFFFFFC0B, "Broker DS Value Size Too Large" },
    { 0xFFFFFC0A, "Broker No Attribute Values" },
    { 0xFFFFFC09, "Broker Unknown Session" },
    { 0xFFFFFC08, "Broker Service Disabled" },
    { 0xFFFFFC07, "Broker Unknown Modify Operation" },
    { 0xFFFFFC06, "Broker Invalid Arguments" },
    { 0xFFFFFC05, "Broker Duplicate Session ID" },
    { 0xFFFFFC04, "Broker Unknown Service" },
    { 0xFFFFFC03, "Broker Service Already Enabled" },
    { 0xFFFFFC02, "Broker Service Already Disabled" },
    { 0xFFFFFC01, "Broker Invalid Credential" },
    { 0xFFFFFC00, "Broker Unknown Designator" },
    { 0xFFFFFBFF, "Broker Failed to Make Change Permanent" },
    { 0xFFFFFBFE, "Broker Not Admin Type Session" },
    { 0xFFFFFBFD, "Broker Option Not Supported" },
    { 0xFFFFFBFC, "Broker No Effective Rights" },
    { 0xFFFFFBFB, "Broker Could Not Find File" },
    { 0xFFFFFBFA, "Broker Error Reading File" },
    { 0xFFFFFBF9, "Broker Not NLM File Format" },
    { 0xFFFFFBF8, "Broker Wrong NLM File Version" },
    { 0xFFFFFBF7, "Broker Reentrant Initialization Failure" },
    { 0xFFFFFBF6, "Broker Already in Progress" },
    { 0xFFFFFBF5, "Broker Initialize Failure" },
    { 0xFFFFFBF4, "Broker Inconsistent File Format" },
    { 0xFFFFFBF3, "Broker Can't Load at Startup" },
    { 0xFFFFFBF2, "Broker Autoload Modules Not Loaded" },
    { 0xFFFFFBF1, "Broker Unresolved External" },
    { 0xFFFFFBF0, "Broker Public Already Defined" },
    { 0xFFFFFBEF, "Broker Other Broker Using Object" },
    { 0xFFFFFBEE, "Broker Service Failed to Initialize" },
    { 0xFFFFFBB4, "Registry Out of Memory" },       /* SRS Errors */
    { 0xFFFFFBB3, "Registry Bad NetWare Version" },
    { 0xFFFFFBB2, "Registry Failed to Create Context" },
    { 0xFFFFFBB1, "Registry Failed Login" },
    { 0xFFFFFBB0, "Registry Failed to Create Thread" },
    { 0xFFFFFBAF, "Registry Failed to Get Messages" },
    { 0xFFFFFBAE, "Registry Service Name Must Be Fully Distinguished" },
    { 0xFFFFFBAD, "Registry DS Value Size Too Large" },
    { 0xFFFFFBAC, "Registry No Attribute Values" },
    { 0xFFFFFBAB, "Registry Unknown Session" },
    { 0xFFFFFBAA, "Registry Service Disabled" },
    { 0xFFFFFBA9, "Registry Unknown Modify Operation" },
    { 0xFFFFFBA8, "Registry Can't Start Advertise" },
    { 0xFFFFFBA7, "Registry Duplicate Server Entry" },
    { 0xFFFFFBA6, "Registry Can't Bind to Registry" },
    { 0xFFFFFBA5, "Registry Can't Create Client" },
    { 0xFFFFFBA4, "Registry Invalid Arguments" },
    { 0xFFFFFBA3, "Registry Duplicate Session ID" },
    { 0xFFFFFBA2, "Registry Unknown Server Entry" },
    { 0xFFFFFBA1, "Registry Invalid Credential" },
    { 0xFFFFFBA0, "Registry Type Session" },
    { 0xFFFFFB9F, "Registry Server Type Session" },
    { 0xFFFFFB9E, "Registry Not Server Type Session" },
    { 0xFFFFFB9D, "Not Registry Type Session" },
    { 0xFFFFFB9C, "Registry Unknown Designator" },
    { 0xFFFFFB9B, "Registry Option Not Supported" },
    { 0xFFFFFB9A, "Registry Not in List Iteration" },
    { 0xFFFFFB99, "Registry Invalid Continuation Handle" },
    { 0xFFFFFB50, "Notify Out of Memory" },        /* Notification Service Errors */
    { 0xFFFFFB4F, "Notify Bad NetWare Version" },
    { 0xFFFFFB4E, "Notify Failed to Create Thread" },
    { 0xFFFFFB4D, "Notify Failed to Get Messages" },
    { 0xFFFFFB4C, "Notify Failed to Create Context" },
    { 0xFFFFFB4B, "Notify Failed Login" },
    { 0xFFFFFB4A, "Notify Service Name Must be Fully Distiguished" },
    { 0xFFFFFB49, "Notify DS Value Size Too Large" },
    { 0xFFFFFB48, "Notify No Attribute Values" },
    { 0xFFFFFB47, "Notify Unknown Session" },
    { 0xFFFFFB46, "Notify Unknown Notify Profile" },
    { 0xFFFFFB45, "Notify Error Reading File" },
    { 0xFFFFFB44, "Notify Error Writing File" },
    { 0xFFFFFB43, "Wrong Notify Database Version" },
    { 0xFFFFFB42, "Corrupted Notify Database" },
    { 0xFFFFFB41, "Notify Unknown Event Object ID" },
    { 0xFFFFFB40, "Notify Method Already Installed" },
    { 0xFFFFFB3F, "Notify Unknown Method" },
    { 0xFFFFFB3E, "Notify Service Disabled" },
    { 0xFFFFFB3D, "Notify Unknown Modify Operation" },
    { 0xFFFFFB3C, "Out of Notify Entries" },
    { 0xFFFFFB3B, "Notify Unknown Language ID" },
    { 0xFFFFFB3A, "Notify Queue Empty" },
    { 0xFFFFFB39, "Notify Can't Load Delivery Method" },
    { 0xFFFFFB38, "Notify Invalid Arguments" },
    { 0xFFFFFB37, "Notify Duplicate Session ID" },
    { 0xFFFFFB36, "Notify Invalid Credentials" },
    { 0xFFFFFB35, "Notify Unknown Choice" },
    { 0xFFFFFB34, "Notify Unknown Attribute Value" },
    { 0xFFFFFB33, "Notify Error Writing Database" },
    { 0xFFFFFB32, "Notify Unknown Object ID" },
    { 0xFFFFFB31, "Notify Unknown Designator" },
    { 0xFFFFFB30, "Notify Failed to Make Change Permanent" },
    { 0xFFFFFB2F, "Notify User Interface Not Supported" },
    { 0xFFFFFB2E, "Notify Not Supplied Type of Session" },
    { 0xFFFFFB2D, "Notify Not Admin Type Session" },
    { 0xFFFFFB2C, "Notify No Service Registry Available" },
    { 0xFFFFFB2B, "Notify Failed to Register With Any Server" },
    { 0xFFFFFB2A, "Notify Empty Event Object Set" },
    { 0xFFFFFB29, "Notify Unknown Notify Handle" },
    { 0xFFFFFB28, "Notify Option Not Supported" },
    { 0xFFFFFB27, "Notify Unknown RPC Session" },
    { 0xFFFFFB26, "Notify Initialization Error" },
    { 0xFFFFFB25, "Notify No Effective Rights" },
    { 0xFFFFFB24, "Notify No Persistent Storage" },
    { 0xFFFFFB23, "Notify Bad Method Filename" },
    { 0xFFFFFB22, "Notify Unknown Continuation Handle" },
    { 0xFFFFFB21, "Notify Invalid Continuation Handle" },
    { 0xFFFFFB20, "Notify Could Not Find File" },
    { 0xFFFFFB1F, "Notify Error Reading File" },
    { 0xFFFFFB1E, "Notify Not NLM File Format" },
    { 0xFFFFFB1D, "Notify Wrong NLM File Version" },
    { 0xFFFFFB1C, "Notify Reentrant Initialization Failure" },
    { 0xFFFFFB1B, "Notify Already in Progress" },
    { 0xFFFFFB1A, "Notify Initialization Failure" },
    { 0xFFFFFB19, "Notify Inconsistent File Format" },
    { 0xFFFFFB18, "Notify Can't Load at Startup" },
    { 0xFFFFFB17, "Notify Autoload Modules Not Loaded" },
    { 0xFFFFFB16, "Notify Unresolved External" },
    { 0xFFFFFB15, "Notify Public Already Defined" },
    { 0xFFFFFB14, "Notify Using Unknown Methods" },
    { 0xFFFFFB13, "Notify Service Not Fully Enabled" },
    { 0xFFFFFB12, "Notify Foreign NDS Tree Name" },
    { 0xFFFFFB11, "Notify Delivery Method Rejected Address" },
    { 0xFFFFFB10, "Notify Unsupported Delivery Address Type" },
    { 0xFFFFFB0F, "Notify User Object No Default Server" },
    { 0xFFFFFB0E, "Notify Failed to Send Notification" },
    { 0xFFFFFB0D, "Notify Bad Volume in Address" },
    { 0xFFFFFB0C, "Notify Broker Has No File Rights" },
    { 0xFFFFFB0B, "Notify Maximum Methods Supported" },
    { 0xFFFFFB0A, "Notify No Filter Provided" },
    { 0xFFFFFB09, "Notify IPX Not Supported By Method" },
    { 0xFFFFFB08, "Notify IP Not Supported By Method" },
    { 0xFFFFFB07, "Notify Failed to Startup Winsock" },
    { 0xFFFFFB06, "Notify No Protocols Available" },
    { 0xFFFFFB05, "Notify Failed to Launch RPC Server" },
    { 0xFFFFFB04, "Notify Invalid SLP Attribute Format" },
    { 0xFFFFFB03, "Notify Invalid SLP URL Format" },
    { 0xFFFFFB02, "Notify Unknown Attribute Object ID" },
    { 0xFFFFFB01, "Notify Duplicate Session ID" },
    { 0xFFFFFB00, "Notify Failed to Authenticate" },
    { 0xFFFFFAFF, "Notify Failed to Authenticate Protocol Mismatch" },
    { 0xFFFFFAFE, "Notify Failed to Authenticate Internal Error" },
    { 0xFFFFFAFD, "Notify Failed to Authenticate Connection Error" },
    { 0xFFFFFC7C, "Resource Manager Out of Memory" },  /* ResMan Errors */
    { 0xFFFFFC7B, "Resource Manager Bad NetWare Version" },
    { 0xFFFFFC7A, "Resource Manager Wrong Command Line Arguments" },
    { 0xFFFFFC79, "Resource Manager Broker Name Not Given" },
    { 0xFFFFFC78, "Resource Manager Invalid Broker Password" },
    { 0xFFFFFC77, "Resource Manager Invalid Broker Name" },
    { 0xFFFFFC76, "Resource Manager Failed to Create Thread" },
    { 0xFFFFFC75, "Resource Manager Service Name Must be Fully Distinguished" },
    { 0xFFFFFC74, "Resource Manager DS Value Size Too Large" },
    { 0xFFFFFC73, "Resource Manager No Attribute Values" },
    { 0xFFFFFC72, "Resource Manager Unknown Session" },
    { 0xFFFFFC71, "Resource Manager Error Reading File" },
    { 0xFFFFFC70, "Resource Manager Error Writing File" },
    { 0xFFFFFC6F, "Resource Manager Service Disabled" },
    { 0xFFFFFC6E, "Resource Manager Unknown Modify Operation" },
    { 0xFFFFFC6D, "Resource Manager Duplicate Session ID" },
    { 0xFFFFFC6C, "Resource Manager Invalid Credentials" },
    { 0xFFFFFC6B, "Resource Manager No Service Registry Available" },
    { 0xFFFFFC6A, "Resource Manager Failed to Register With any Server" },
    { 0xFFFFFC69, "Resource Manager Failed to Get Messages" },
    { 0xFFFFFC68, "Resource Manager Failed to Create Context" },
    { 0xFFFFFC67, "Resource Manager Failed to Login" },
    { 0xFFFFFC66, "Resource Manager NPD Files Generation Error" },
    { 0xFFFFFC65, "Resource Manager INF File Format Error" },
    { 0xFFFFFC64, "Resource Manager No Printer Type in INF File" },
    { 0xFFFFFC63, "Resource Manager No INF Files Present" },
    { 0xFFFFFC62, "Resource Manager File Open Error" },
    { 0xFFFFFC61, "Resource Manager Read File Error" },
    { 0xFFFFFC60, "Resource Manager Write File Error" },
    { 0xFFFFFC5F, "Resource Manager Resource Type Invalid" },
    { 0xFFFFFC5E, "Resource Manager No Such Filename" },
    { 0xFFFFFC5D, "Resource Manager Banner Type Invalid" },
    { 0xFFFFFC5C, "Resource Manager List Type Unknown" },
    { 0xFFFFFC5B, "Resource Manager OS Not Supported" },
    { 0xFFFFFC5A, "Resource Manager No Banner Files Present" },
    { 0xFFFFFC59, "Resource Manager Printer Definition Type Unknown" },
    { 0xFFFFFC58, "Resource Manager No Printer Types in List" },
    { 0xFFFFFC57, "Resource Manager Option Not Supported" },
    { 0xFFFFFC56, "Resource Manager Unicode Convention Error" },
    { 0xFFFFFC55, "Resource Manager Invalid Arguments" },
    { 0xFFFFFC54, "Resource Manager Initialization Error" },
    { 0xFFFFFC53, "Resource Manager No Service Registry Available" },
    { 0xFFFFFC52, "Resource Manager Failed to Register to Any Server" },
    { 0xFFFFFC51, "Resource Manager Unknown Designator" },
    { 0xFFFFFC50, "Resource Manager Not Admin Session" },
    { 0xFFFFFC4F, "Resource Manager No Effective Rights" },
    { 0xFFFFFC4E, "Resource Manager Bad File Attribute" },
    { 0xFFFFFC4D, "Resource Manager Document ID Format Error" },
    { 0xFFFFFC4C, "Resource Manager Unknown RPC Session" },
    { 0xFFFFFC4B, "Resource Manager Session Being Removed" },
    { 0xFFFFFC49, "Resource Manager Font Manager IO Error" },
    { 0xFFFFFC48, "Resource Manager Font Manager Reentrancy" },
    { 0xFFFFFC47, "Resource Manager Font Manager Sequence Error" },
    { 0xFFFFFC46, "Resource Manager Font Manager Corrupt Index File" },
    { 0xFFFFFC45, "Resource Manager Font Manager No Such Font" },
    { 0xFFFFFC44, "Resource Manager Font Manager Not Initialized" },
    { 0xFFFFFC43, "Resource Manager Font Manager System Error" },
    { 0xFFFFFC42, "Resource Manager Font Manager Bad Parameter" },
    { 0xFFFFFC41, "Resource Manager Font Manager Path Too Long" },
    { 0xFFFFFC40, "Resource Manager Font Manager Failure" },
    { 0xFFFFFC3F, "Resource Manager Duplicate TIRPC Session" },
    { 0xFFFFFC3E, "Resource Manager Connection Lost RMS Data" },
    { 0xFFFFFC3D, "Resource Manager Failed to Start Winsock" },
    { 0xFFFFFC3C, "Resource Manager No Protocols Available" },
    { 0xFFFFFC3B, "Resource Manager Failed to Launch RPC Server" },
    { 0xFFFFFC3A, "Resource Manager Invalid SLP Attribute Format" },
    { 0xFFFFFC39, "Resource Manager Invalid SLP URL Format" },
    { 0xFFFFFC38, "Resource Manager Unresolved External" },
    { 0xFFFFFC37, "Resource Manager Failed to Authenticate" },
    { 0xFFFFFC36, "Resource Manager Failed to Authenticate Protocol Mismatch" },
    { 0xFFFFFC35, "Resource Manager Failed to Authenticate Internal Error" },
    { 0xFFFFFC34, "Resource Manager Failed to Authenticate Connection Error" },
    { 0xFFFFFC33, "Resource Manager No Rights to Remote Resdir" },
    { 0xFFFFFC32, "Resource Manager Can't Initialize NDPS Library" },
    { 0xFFFFFC31, "Resource Manager Can't Create Resource Reference" },
    { 0xFFFFFC30, "Resource Manager File is Zero Length" },
    { 0xFFFFFC2F, "Resource Manager Failed to Write INF in Address" },
    { 0xFFFFFCDF, "NDPSM No Memory" },               /* NDPSM Errors */
    { 0xFFFFFCDE, "NDPSM Memory Not Found" },
    { 0xFFFFFCDD, "NDPSM Job Storage Limit" },
    { 0xFFFFFCDC, "NDPSM Job Retention Limit" },
    { 0xFFFFFCDB, "NDPSM Unsupported Type" },
    { 0xFFFFFCDA, "NDPSM Undefined Type" },
    { 0xFFFFFCD9, "NDPSM Unsupported Operation" },
    { 0xFFFFFCD8, "NDPSM Error Accessing Database" },
    { 0xFFFFFCD7, "NDPSM No PDS" },
    { 0xFFFFFCD6, "NDPSM Invalid Class" },
    { 0xFFFFFCD5, "NDPSM Bad Parameter" },
    { 0xFFFFFCD4, "NDPSM Object Not Found" },
    { 0xFFFFFCD3, "NDPSM Attribute Not Found" },
    { 0xFFFFFCD2, "NDPSM Value Not Found" },
    { 0xFFFFFCD1, "NDPSM Values Not Comparable" },
    { 0xFFFFFCD0, "NDPSM Invalid Value Syntax" },
    { 0xFFFFFCCF, "NDPSM Job Not Found" },
    { 0xFFFFFCCE, "NDPSM Communications Error" },
    { 0xFFFFFCCD, "NDPSM Printer Agent Initializing" },
    { 0xFFFFFCCC, "NDPSM Printer Agent Going Down" },
    { 0xFFFFFCCB, "NDPSM Printer Agent Disabled" },
    { 0xFFFFFCCA, "NDPSM Printer Agent Paused" },
    { 0xFFFFFCC9, "NDPSM Bad Printer Agent Handle" },
    { 0xFFFFFCC8, "NDPSM Object Not Locked" },
    { 0xFFFFFCC7, "NDPSM Version Incompatible" },
    { 0xFFFFFCC6, "NDPSM PSM Initializing" },
    { 0xFFFFFCC5, "NDPSM PSM Going Down" },
    { 0xFFFFFCC4, "NDPSM Notification Service Error" },
    { 0xFFFFFCC3, "NDPSM Medium Needs Mounted" },
    { 0xFFFFFCC2, "NDPSM PDS Not Responding" },
    { 0xFFFFFCC1, "NDPSM Session Not Found" },
    { 0xFFFFFCC0, "NDPSM RPC Failure" },
    { 0xFFFFFCBF, "NDPSM Duplicate Value" },
    { 0xFFFFFCBE, "NDPSM PDS Refuses Rename" },
    { 0xFFFFFCBD, "NDPSM No Mandatory Attribute" },
    { 0xFFFFFCBC, "NDPSM Already Attached" },
    { 0xFFFFFCBB, "NDPSM Can't Attach" },
    { 0xFFFFFCBA, "NDPSM Too Many NetWare Servers" },
    { 0xFFFFFCB9, "NDPSM Can't Create Document File" },
    { 0xFFFFFCB8, "NDPSM Can't Delete Document File" },
    { 0xFFFFFCB7, "NDPSM Can't Open Document File" },
    { 0xFFFFFCB6, "NDPSM Can't Write Document File" },
    { 0xFFFFFCB5, "NDPSM Job is Active" },
    { 0xFFFFFCB4, "NDPSM No Scheduler" },
    { 0xFFFFFCB3, "NDPSM Changing Connection" },
    { 0xFFFFFCB2, "NDPSM Could not Create Account Reference" },
    { 0xFFFFFCB1, "NDPSM Accounting Service Error" },
    { 0xFFFFFCB0, "NDPSM RMS Service Error" },
    { 0xFFFFFCAF, "NDPSM Failed Validation" },
    { 0xFFFFFCAE, "NDPSM Broker Server Connecting" },
    { 0xFFFFFCAD, "NDPSM SRS Service Error" },
    { 0xFFFFFD44, "JPM Execute Request Later" },
    { 0xFFFFFD43, "JPM Failed to Open Document" },
    { 0xFFFFFD42, "JPM Failed to Read Document File" },
    { 0xFFFFFD41, "JPM Bad Printer Agent Handle" },
    { 0xFFFFFD40, "JPM Bad Job Handle" },
    { 0xFFFFFD3F, "JPM Bad Document Handle" },
    { 0xFFFFFD3E, "JPM Unsupported Operation" },
    { 0xFFFFFD3D, "JPM Request Queue Full" },
    { 0xFFFFFD3C, "JPM Printer Agent Not Found" },
    { 0xFFFFFD3B, "JPM Invalid Request" },
    { 0xFFFFFD3A, "JPM Not Accepting Requests" },
    { 0xFFFFFD39, "JPM Printer Agent Already Serviced By PDS" },
    { 0xFFFFFD38, "JPM No Job" },
    { 0xFFFFFD37, "JPM Job Not Found" },
    { 0xFFFFFD36, "JPM Could not Access Database" },
    { 0xFFFFFD35, "JPM Bad Object Type" },
    { 0xFFFFFD34, "JPM Job Already Closed" },
    { 0xFFFFFD33, "JPM Document Already Closed" },
    { 0xFFFFFD32, "JPM Print Handler Not Registered" },
    { 0xFFFFFD31, "JPM Version Incompatible" },
    { 0xFFFFFD30, "JPM Printer Agent Paused" },
    { 0xFFFFFD2F, "JPM Printer Agent Shutdown" },
    { 0xFFFFFD2E, "JPM No CLIB Context" },
    { 0xFFFFFD2D, "JPM Accounting Already Serviced" },
    { 0xFFFFFC7B, "Database Can't Create File" },
    { 0xFFFFFC7A, "Database Can't Find Data File" },
    { 0xFFFFFC79, "Database Can't Open Data File" },
    { 0xFFFFFC78, "Database Can't Open Index File" },
    { 0xFFFFFC77, "Database Index File Not Open" },
    { 0xFFFFFC76, "Database Can't Rename File" },
    { 0xFFFFFC75, "Database Can't Read Data File" },
    { 0xFFFFFC74, "Database Can't Read Index File" },
    { 0xFFFFFC73, "Database Can't Write Data File" },
    { 0xFFFFFC72, "Database Can't Write Index File" },
    { 0xFFFFFC71, "Database Can't Delete Printer Agent Directory" },
    { 0xFFFFFC70, "Database Already Deleted" },
    { 0xFFFFFC6F, "Database Object Exists" },
    { 0xFFFFFC6E, "Database Descriptor In Use" },
    { 0xFFFFFC6D, "Database Descriptor Being Deleted" },
    { 0,          NULL }
};

static const value_string ndps_credential_enum[] = {
    { 0, "SIMPLE" },
    { 1, "CERTIFIED" },
    { 2, "NDPS 0" },
    { 3, "NDPS 1" },
    { 4, "NDPS 2" },
    { 0, NULL }
};

static const value_string ndps_object_op_enum[] = {
    { 0, "None" },
    { 1, "Add" },
    { 2, "Delete" },
    { 3, "Delete Object" },
    { 0, NULL }
};

static const value_string ndps_client_server_enum[] = {
    { 0, "Client" },
    { 1, "Server" },
    { 2, "Client and Server" },
    { 0, NULL }
};

static const value_string ndps_session_type_enum[] = {
    { 0, "Unknown" },
    { 1, "User" },
    { 2, "Admin" },
    { 3, "Server" },
    { 4, "Registry" },
    { 0, NULL }
};

static const value_string ndps_get_session_type_enum[] = {
    { 0, "Unknown" },
    { 1, "User" },
    { 2, "Admin" },
    { 3, "Supplier" },
    { 0, NULL }
};

static const value_string ndps_get_resman_session_type_enum[] = {
    { 0, "Unknown" },
    { 1, "User" },
    { 2, "Admin" },
    { 0, NULL }
};

static int
align_4(tvbuff_t *tvb, int aoffset)
{
       if(tvb_length_remaining(tvb, aoffset) > 4 )
       {
                return (aoffset%4);
       }
       return 0;
}

static int
ndps_string(tvbuff_t* tvb, int hfinfo, proto_tree *ndps_tree, int offset)
{
        int     foffset = offset;
        guint32 str_length;
        char    buffer[1024];
        guint32 i;
        guint16 c_char;
        guint32 length_remaining = 0;
        
        str_length = tvb_get_ntohl(tvb, foffset);
        foffset += 4;
        length_remaining = tvb_length_remaining(tvb, foffset);
        g_assert(length_remaining > 0);
        if(str_length > (guint)length_remaining || str_length > 1024)
        {
                proto_tree_add_string(ndps_tree, hfinfo, tvb, foffset,
                    length_remaining + 4, "<String too long to process>");
                foffset += length_remaining;
                return foffset;
        }
        if(str_length == 0)
        {
       	    proto_tree_add_string(ndps_tree, hfinfo, tvb, offset,
                4, "<Not Specified>");
            return foffset;
        }
        for ( i = 0; i < str_length; i++ )
        {
                c_char = tvb_get_guint8(tvb, foffset );
                if (c_char<0x20 || c_char>0x7e)
                {
                        if (c_char != 0x00)
                        { 
                                c_char = 0x2e;
                                buffer[i] = c_char & 0xff;
                        }
                        else
                        {
                                i--;
                                str_length--;
                        }
                }
                else
                {
                        buffer[i] = c_char & 0xff;
                }
                foffset++;
                length_remaining--;
                
                if(length_remaining==1)
                {
                	i++;
                	break;
                }        
        }
        buffer[i] = '\0';
        
        str_length = tvb_get_ntohl(tvb, offset);
        proto_tree_add_string(ndps_tree, hfinfo, tvb, offset+4,
                str_length, buffer);
        foffset += align_4(tvb, foffset);
        return foffset;
}

static int
objectidentifier(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint32  length=0;
 
    if (tvb_get_ntohl(tvb, foffset)==0) 
    {
        return foffset;
    }
    proto_tree_add_item(ndps_tree, hf_oid_struct_size, tvb, foffset, 4, FALSE);
    foffset += 4;
    proto_tree_add_item(ndps_tree, hf_oid_asn1_type, tvb, foffset, 1, FALSE);
    foffset += 1;
    length = tvb_get_guint8(tvb, foffset);
    foffset += 1;
    proto_tree_add_item(ndps_tree, hf_ndps_oid, tvb, foffset, length, FALSE);
    foffset += length;
    return foffset+(length%2);
}

static int
name_or_id(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint8  length=0;

    proto_tree_add_item(ndps_tree, hf_ndps_nameorid, tvb, foffset, 4, FALSE);
    foffset += 4;
    if(tvb_get_ntohl(tvb, foffset-4)==1) /* Global */
    {
        foffset = objectidentifier(tvb, ndps_tree, foffset);
    }
    else
    {
        if(tvb_get_ntohl(tvb, foffset-4)==2) /* Local */
        {
            foffset = ndps_string(tvb, hf_local_object_name, ndps_tree, foffset);
        }
    }
    return foffset;
}

static int
objectidentification(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint32     h=0;
    guint32     object_count=0;
    guint32     object_type=0;
    guint32     qualified_name_type=0;
    guint32     length=0;
    proto_tree  *atree;
    proto_item  *aitem;
      
    object_type = tvb_get_ntohl(tvb, foffset); 
    aitem = proto_tree_add_item(ndps_tree, hf_obj_id_type, tvb, foffset, 4, FALSE);
    atree = proto_item_add_subtree(aitem, ett_ndps);
    foffset += 4;
    switch(object_type)
    {
        case 0:         /* Printer Contained Object ID */
            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_object, tvb, foffset, 
            4, FALSE);
            foffset += 4;
            break;
        case 1:         /* Document Identifier */
            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
            /*proto_tree_add_item(atree, hf_ndps_object, tvb, foffset, 
            4, FALSE);
            foffset += 4;*/
            proto_tree_add_item(atree, hf_ndps_document_number, tvb, foffset, 
            4, FALSE);
            foffset += 4;
            break;
        case 2:         /* Object Identifier */
            foffset = objectidentifier(tvb, atree, foffset);
            break;
        case 3:         /* Object Name */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            if (foffset > tvb_length_remaining(tvb, foffset)) {
                return foffset;
            }
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 4:         /* Name or Object ID */
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 5:         /* Simple Name */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            break;
        case 6:         /* Printer Configuration Object ID */
            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
            break;
        case 7:         /* Qualified Name */
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            break;
        case 8:         /* Event Object ID */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 
            4, FALSE);
            foffset += 4;
        default:
            break;
    }
    return foffset;
}

static int
address_item(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint32     address_type=0;
    guint32     qualified_name_type;
    guint32     transport_type=0;
    guint32     octet_len=0;

    address_type = tvb_get_ntohl(tvb, foffset); 
    proto_tree_add_uint(ndps_tree, hf_address_type, tvb, foffset, 4, address_type);
    foffset += 4;
    switch(address_type)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        qualified_name_type = tvb_get_ntohl(tvb, foffset);
        proto_tree_add_uint(ndps_tree, hf_ndps_qualified_name, tvb, foffset, 
        4, qualified_name_type);
        foffset += 4;
        if (qualified_name_type != 0) {
            if (qualified_name_type == 1) {
                foffset = ndps_string(tvb, hf_printer_name, ndps_tree, foffset);
            }
            else
            {
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
            }
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        foffset = ndps_string(tvb, hf_object_name, ndps_tree, foffset);
        break;
    case 13:
        proto_tree_add_item(ndps_tree, hf_ndps_attrib_boolean, tvb, foffset, 4, FALSE);
        foffset += 4;
        break;
    case 14:
        proto_tree_add_item(ndps_tree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
        foffset += 4;
        break;
    case 15:
        transport_type=tvb_get_ntohl(tvb, foffset);
        proto_tree_add_item(ndps_tree, hf_ndps_address, tvb, foffset, 4, FALSE);
        foffset += 4;
        octet_len = tvb_get_ntohl(tvb, foffset);
        proto_tree_add_item(ndps_tree, hf_ndps_add_bytes, tvb, foffset, 4, FALSE);
        foffset += octet_len + 4;
        break;
    case 16:
    case 17:
    default:
        foffset = ndps_string(tvb, hf_object_name, ndps_tree, foffset);
        break;
    }
    return foffset;
}

static int
print_address(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint32     address_type=0;
    guint32     address=0;
    guint32     address_len=0;

    address_type = tvb_get_ntohl(tvb, foffset); 
    proto_tree_add_uint(ndps_tree, hf_ndps_address, tvb, foffset, 4, address_type);
    foffset += 4;
    address_len = tvb_get_ntohl(tvb, foffset);
    proto_tree_add_item(ndps_tree, hf_address_len, tvb, foffset, 4, FALSE);
    foffset += 4;
    switch(address_type)
    {
    case 0x00000000:
            proto_tree_add_item(ndps_tree, hf_ndps_net, tvb, foffset, 4, FALSE);
            proto_tree_add_item(ndps_tree, hf_ndps_node, tvb, foffset+4, 6, FALSE);
            proto_tree_add_item(ndps_tree, hf_ndps_socket, tvb, foffset+10, 2, FALSE);
            foffset += address_len;
            break;
    case 0x00000001:
            proto_tree_add_item(ndps_tree, hf_ndps_port, tvb, foffset, 2, FALSE);
            address = tvb_get_letohl(tvb, foffset+2);
            proto_tree_add_ipv4(ndps_tree, hf_ndps_ip, tvb, foffset+2, 4, address);
            foffset += address_len;
            break;
    default:
        foffset += tvb_get_ntohl(tvb, foffset -4);
        break;
    }
    return foffset+(address_len%4);
}


static int
attribute_value(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint8      h;
    guint8      i;
    guint8      j;
    guint8      number_of_values=0;
    guint8      number_of_items=0;
    guint8      number_of_items2=0;
    guint32     attribute_type=0;
    guint32     qualified_name_type=0;
    guint32     integer_or_oid=0;
    guint32     event_object_type=0;
    guint32     ignored_type=0;
    guint32     resource_type=0;
    guint32     identifier_type=0;
    guint32     criterion_type=0;
    guint32     card_enum_time=0;
    guint32     media_type=0;
    guint32     doc_content=0;
    guint32     page_size=0;
    guint32     medium_size=0;
    guint32     numbers_up=0;
    guint32     colorant_set=0;
    guint32     length=0;
    proto_tree  *atree;
    proto_item  *aitem;
    proto_tree  *btree;
    proto_item  *bitem;
    proto_tree  *ctree;
    proto_item  *citem;

    attribute_type = tvb_get_ntohl(tvb, foffset); 
    aitem = proto_tree_add_item(ndps_tree, hf_obj_attribute_type, tvb, foffset, 4, FALSE);
    atree = proto_item_add_subtree(aitem, ett_ndps);
    foffset += 4;
    switch(attribute_type)
    {
        case 0:         /* Null */
            proto_tree_add_item(atree, hf_ndps_data, tvb, foffset+4, tvb_get_ntohl(tvb, foffset), FALSE);
            break;
        case 1:         /* Text */
        case 2:         /* Descriptive Name */
        case 3:         /* Descriptor */
        case 6:         /* Simple Name */
        case 40:         /* Distinguished Name*/
        case 50:         /* Font Reference */
        case 58:         /* Locale */
        case 102:         /* File Path */
        case 103:         /* Uniform Resource Identifier */
        case 108:         /* Extended Resource Identifier */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            break;
        case 4:         /* Message */
        case 5:         /* Error Message */
        case 38:         /* Name or OID */
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 39:         /* Name or OID Seq */
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            break;
        case 7:         /* Distinguished Name String*/
        case 79:         /* File Reference */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 8:         /* Distinguished Name String Seq */
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = ndps_string(tvb, hf_object_name, atree, foffset);
                foffset = name_or_id(tvb, atree, foffset);
            }
            break;
        case 9:          /* Delta Time */
        case 10:         /* Time */
        case 11:         /* Integer */
        case 13:         /* Cardinal */
        case 15:         /* Positive Integer */
        case 18:         /* Maximum Integer */
        case 19:         /* Minimum Integer */
        case 35:         /* Percent */
        case 57:         /* Job Priority */
        case 72:         /* Sides */
        case 95:         /* Enumeration */
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 12:         /* Integer Seq */
        case 14:         /* Cardinal Seq */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_info_int32, tvb, foffset, length, FALSE);
            foffset += length;
            break;
        case 16:         /* Integer Range */
        case 17:         /* Cardinal Range */
            proto_tree_add_item(atree, hf_ndps_lower_range, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_upper_range, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 20:         /* Integer 64 */
        case 22:         /* Cardinal 64 */
        case 24:         /* Positive Integer 64 */
        case 31:         /* Non-Negative Real */
        case 29:         /* Real */
            proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 21:         /* Integer 64 Seq */
        case 23:         /* Cardinal 64 Seq */
        case 30:         /* Real Seq */
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            break;
        case 25:         /* Integer 64 Range */
        case 26:         /* Cardinal 64 Range */
        case 32:         /* Real Range */
        case 33:         /* Non-Negative Real Range */
            proto_tree_add_item(atree, hf_ndps_lower_range_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            proto_tree_add_item(atree, hf_ndps_upper_range_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 27:         /* Maximum Integer 64 */
            proto_tree_add_item(atree, hf_ndps_lower_range_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 28:         /* Minimum Integer 64 */
            proto_tree_add_item(atree, hf_ndps_upper_range_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 34:         /* Boolean */
            proto_tree_add_item(atree, hf_ndps_attrib_boolean, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 36:         /* Object Identifier */
                foffset = objectidentifier(tvb, atree, foffset);
            break;
        case 37:         /* Object Identifier Seq */
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = objectidentifier(tvb, atree, foffset);
            }
            break;
        case 41:         /* Relative Distinguished Name Seq */
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            }
            break;
        case 42:         /* Realization */
            proto_tree_add_item(atree, hf_ndps_realization, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 43:         /* Medium Dimensions */
            proto_tree_add_item(atree, hf_ndps_xdimension_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            proto_tree_add_item(atree, hf_ndps_ydimension_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 44:         /* Dimension */
            proto_tree_add_item(atree, hf_ndps_dim_value, tvb, foffset, 8, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) {
                proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            else
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            proto_tree_add_item(atree, hf_ndps_dim_flag, tvb, foffset, 8, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 45:         /* XY Dimensions */
            proto_tree_add_item(atree, hf_ndps_xydim_value, tvb, foffset, 8, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 1) {
                foffset = name_or_id(tvb, atree, foffset);
            }
            else
            {
                proto_tree_add_item(atree, hf_ndps_xdimension_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ydimension_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            proto_tree_add_item(atree, hf_ndps_dim_flag, tvb, foffset, 8, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 46:         /* Locations */
            proto_tree_add_item(atree, hf_ndps_location_value, tvb, foffset, 8, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) {
                proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
                    foffset += 8;
                }
            }
            else
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            proto_tree_add_item(atree, hf_ndps_dim_flag, tvb, foffset, 8, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 47:         /* Area */
            proto_tree_add_item(atree, hf_ndps_xmin_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            proto_tree_add_item(atree, hf_ndps_xmax_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            proto_tree_add_item(atree, hf_ndps_ymin_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            proto_tree_add_item(atree, hf_ndps_ymax_n64, tvb, foffset, 8, FALSE);
            foffset += 8;
            break;
        case 48:         /* Area Seq */
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_xmin_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_xmax_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ymin_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ymax_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            break;
        case 49:         /* Edge */
            proto_tree_add_item(atree, hf_ndps_edge_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 51:         /* Cardinal or OID */
            proto_tree_add_item(atree, hf_ndps_cardinal_or_oid, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0) {
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            else
            {
                foffset = objectidentifier(tvb, atree, foffset);
            }
            break;
        case 52:         /* OID Cardinal Map */
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 53:         /* Cardinal or Name or OID */
            proto_tree_add_item(atree, hf_ndps_cardinal_name_or_oid, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0) {
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            else
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            break;
        case 54:         /* Positive Integer or OID */
            integer_or_oid = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_integer_or_oid, tvb, foffset, 4, integer_or_oid);
            foffset += 4;
            if (integer_or_oid==0) {
                foffset = objectidentifier(tvb, atree, foffset);
            }
            else
            {
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            break;
        case 55:         /* Event Handling Profile */
            proto_tree_add_item(atree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_persistence, tvb, foffset, 4, FALSE);
            foffset += 4;
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
            foffset += length;
            proto_tree_add_item(atree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = name_or_id(tvb, atree, foffset);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset += address_item(tvb, atree, foffset);
            }
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = objectidentifier(tvb, atree, foffset);
                foffset = objectidentification(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                
                event_object_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, event_object_type);
                foffset += 4;
                if(event_object_type==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(event_object_type==0)
                    {
                        number_of_items2 = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items2);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items2; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
            }
            break;
        case 56:         /* Octet String */
        case 63:         /* Job Password */
        case 66:         /* Print Checkpoint */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
            foffset += length;
            foffset += (length%2);
            break;
        case 59:         /* Method Delivery Address */
            proto_tree_add_item(atree, hf_ndps_delivery_add_type, tvb, foffset, 4, FALSE);
            event_object_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch(event_object_type)
            {
                case 0:     /*MHS ADDR*/
                case 1:     /*DISTINGUISHED_NAME*/
                case 2:     /*TEXT*/
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    break;
                case 3:     /*OCTET_STRING*/
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
                    foffset += length;
                    foffset += (length%2);
                    break;
                case 4:     /*DIST_NAME_STRING*/
                    foffset = ndps_string(tvb, hf_object_name, atree, foffset);
                    foffset = name_or_id(tvb, atree, foffset);
                    break;
                case 5:     /*RPC_ADDRESS*/
                case 6:     /*QUALIFIED_NAME*/
                    foffset = objectidentifier(tvb, atree, foffset);
                    qualified_name_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                    4, qualified_name_type);
                    foffset += 4;
                    if (qualified_name_type != 0) {
                        if (qualified_name_type == 1) {
                            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                        }
                        else
                        {
                            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                            foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                        }
                    }
                    break;
                default:
                    break;
            }
            break;
        case 60:         /* Object Identification */
            foffset = objectidentification(tvb, atree, foffset);
            break;
        case 61:         /* Results Profile */
            foffset = objectidentifier(tvb, atree, foffset);
            foffset = name_or_id(tvb, atree, foffset);
            foffset = address_item(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 62:         /* Criteria */
            foffset = objectidentifier(tvb, atree, foffset);
            criterion_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_criterion_type, tvb, foffset, 4, criterion_type);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 64:         /* Job Level */
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 65:         /* Job Categories */
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
            }
            break;
        case 67:         /* Ignored Attribute */
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                ignored_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_ignored_type, tvb, foffset, 4, ignored_type);
                foffset += 4;
                if (ignored_type == 38)
                {
                    foffset = name_or_id(tvb, atree, foffset);
                }
                else
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
            }
            break;
        case 68:         /* Resource */
            resource_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_resource_type, tvb, foffset, 4, resource_type);
            foffset += 4;
            if (resource_type == 0)
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            else
            {
                foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
            }
            break;
        case 69:         /* Medium Substitution */
            foffset = name_or_id(tvb, atree, foffset);
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 70:         /* Font Substitution */
            foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
            foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
            break;
        case 71:         /* Resource Context Seq */
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                resource_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_resource_type, tvb, foffset, 4, resource_type);
                foffset += 4;
                if (resource_type == 0)
                {
                    foffset = name_or_id(tvb, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            break;
        case 73:         /* Page Select Seq */
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_page_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                identifier_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_identifier_type, tvb, foffset, 4, identifier_type);
                foffset += 4;
                if (identifier_type == 0)
                {
                    proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                if (identifier_type == 1)
                {
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
                if (identifier_type == 2)
                {
                    foffset = name_or_id(tvb, atree, foffset);
                }
                proto_tree_add_item(atree, hf_ndps_page_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                identifier_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_identifier_type, tvb, foffset, 4, identifier_type);
                foffset += 4;
                if (identifier_type == 0)
                {
                    proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                if (identifier_type == 1)
                {
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
                if (identifier_type == 2)
                {
                    foffset = name_or_id(tvb, atree, foffset);
                }
            }
            break;
        case 74:         /* Page Media Select */
            media_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_media_type, tvb, foffset, 4, media_type);
            foffset += 4;
            if (media_type == 0)
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            else
            {
                foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    proto_tree_add_item(atree, hf_ndps_page_flag, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    identifier_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_uint(atree, hf_ndps_identifier_type, tvb, foffset, 4, identifier_type);
                    foffset += 4;
                    if (identifier_type == 0)
                    {
                        proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                        foffset += 4;
                    }
                    if (identifier_type == 1)
                    {
                        foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    }
                    if (identifier_type == 2)
                    {
                        foffset = name_or_id(tvb, atree, foffset);
                    }
                }
            }
            break;
        case 75:         /* Document Content */
            doc_content = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_doc_content, tvb, foffset, 4, doc_content);
            foffset += 4;
            if (doc_content == 0)
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
            }
            else
            {
                foffset = ndps_string(tvb, hf_object_name, atree, foffset);
                foffset = name_or_id(tvb, atree, foffset);
            }
            break;
        case 76:         /* Page Size */
            page_size = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_page_size, tvb, foffset, 4, page_size);
            foffset += 4;
            if (page_size == 0)
            {
                foffset = objectidentifier(tvb, atree, foffset);
            }
            else
            {
                proto_tree_add_item(atree, hf_ndps_xdimension_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ydimension_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            break;
        case 77:         /* Presentation Direction */
            proto_tree_add_uint(atree, hf_ndps_direction, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 78:         /* Page Order */
            proto_tree_add_uint(atree, hf_ndps_page_order, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 80:         /* Medium Source Size */
            foffset = name_or_id(tvb, atree, foffset);
            medium_size = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_medium_size, tvb, foffset, 4, medium_size);
            foffset += 4;
            if (medium_size == 0)
            {
                page_size = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_page_size, tvb, foffset, 4, page_size);
                foffset += 4;
                if (page_size == 0)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    proto_tree_add_item(atree, hf_ndps_xdimension_n64, tvb, foffset, 8, FALSE);
                    foffset += 8;
                    proto_tree_add_item(atree, hf_ndps_ydimension_n64, tvb, foffset, 8, FALSE);
                    foffset += 8;
                }
                proto_tree_add_item(atree, hf_ndps_long_edge_feeds, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_xmin_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_xmax_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ymin_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ymax_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            else
            {
                proto_tree_add_item(atree, hf_ndps_lower_range_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_upper_range_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_inc_across_feed, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_lower_range_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_upper_range_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_size_inc_in_feed, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_long_edge_feeds, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_xmin_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_xmax_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ymin_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
                proto_tree_add_item(atree, hf_ndps_ymax_n64, tvb, foffset, 8, FALSE);
                foffset += 8;
            }
            break;
        case 81:         /* Input Tray Medium */
            foffset = name_or_id(tvb, atree, foffset);
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 82:         /* Output Bins Characteristics */
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_uint(atree, hf_ndps_page_order, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_uint(atree, hf_ndps_page_orientation, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            break;
        case 83:         /* Page ID Type */
            proto_tree_add_uint(atree, hf_ndps_identifier_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 84:         /* Level Range */
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_lower_range, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_upper_range, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 85:         /* Category Set */
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
            }
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
            }
            break;
        case 86:         /* Numbers Up Supported */
            proto_tree_add_uint(atree, hf_ndps_numbers_up, tvb, foffset, 4, FALSE);
            numbers_up=tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch(numbers_up)
            {
            case 0:     /*Cardinal*/
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            case 1:     /*Name or OID*/
                foffset = name_or_id(tvb, atree, foffset);
                break;
            case 2:     /*Cardinal Range*/
                proto_tree_add_item(atree, hf_ndps_lower_range, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_upper_range, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            break;
        case 87:         /* Finishing */
        case 88:         /* Print Contained Object ID */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 89:         /* Print Config Object ID */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            break;
        case 90:         /* Typed Name */
            foffset = ndps_string(tvb, hf_object_name, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 91:         /* Network Address */
            proto_tree_add_item(atree, hf_ndps_address, tvb, foffset, 4, FALSE);
            foffset += 4;
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_add_bytes, tvb, foffset, 4, FALSE);
            foffset += length;
            break;
        case 92:         /* XY Dimensions Value */
            proto_tree_add_item(atree, hf_ndps_xydim_value, tvb, foffset, 8, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 1) {
                foffset = name_or_id(tvb, atree, foffset);
            }
            else
            {
                if (tvb_get_ntohl(tvb, foffset-4) == 0) 
                {
                    proto_tree_add_item(atree, hf_ndps_xdimension_n64, tvb, foffset, 8, FALSE);
                    foffset += 8;
                    proto_tree_add_item(atree, hf_ndps_ydimension_n64, tvb, foffset, 8, FALSE);
                    foffset += 8;
                }
                else
                {
                    proto_tree_add_item(atree, hf_ndps_xdimension, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    proto_tree_add_item(atree, hf_ndps_ydimension, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
            }
            break;
        case 93:         /* Name or OID Dimensions Map */
            foffset = name_or_id(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_xdimension, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_ydimension, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 94:         /* Printer State Reason */
            foffset = name_or_id(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_state_severity, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_training, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = objectidentifier(tvb, atree, foffset);
            foffset = objectidentification(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = name_or_id(tvb, atree, foffset);
            break;
        case 96:         /* Qualified Name */
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            break;
        case 97:         /* Qualified Name Set */
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                qualified_name_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                4, qualified_name_type);
                foffset += 4;
                if (qualified_name_type != 0) {
                    if (qualified_name_type == 1) {
                        foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                        foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    }
                }
            }
            break;
        case 98:         /* Colorant Set */
            colorant_set = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(atree, hf_ndps_colorant_set, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (colorant_set==0)
            {
                foffset = name_or_id(tvb, atree, foffset);
            }
            else
            {

                foffset = objectidentifier(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = name_or_id(tvb, atree, foffset);
                }
            }
            break;
        case 99:         /* Resource Printer ID */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = ndps_string(tvb, hf_ndps_printer_type, atree, foffset);
                foffset = ndps_string(tvb, hf_ndps_printer_manuf, atree, foffset);
                foffset = ndps_string(tvb, hf_ndps_inf_file_name, atree, foffset);
            }
            proto_tree_add_item(atree, hf_os_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 100:         /* Event Object ID */
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            foffset = objectidentifier(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
            foffset +=4;
            break;
        case 101:         /* Qualified Name Map */
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            break;
        case 104:         /* Cardinal or Enum or Time */
            card_enum_time = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(atree, hf_ndps_card_enum_time, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (card_enum_time==0)
            {
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            else
            {
                if (card_enum_time==1)
                {
                    proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                else
                {
                    proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
            }
            break;
        case 105:         /* Print Contained Object ID Set */
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            break;
        case 106:         /* Octet String Pair */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
            foffset += length;
            foffset += (length%2);
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
            foffset += length;
            foffset += (length%2);
            break;
        case 107:         /* Octet String Integer Pair */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
            foffset += length;
            foffset += (length%2);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 109:         /* Event Handling Profile 2 */
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_persistence, tvb, foffset, 4, FALSE);
            foffset += 4;
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
            foffset += length;
            foffset += (length%2);
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = name_or_id(tvb, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_delivery_add_type, tvb, foffset, 4, FALSE);
            event_object_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch(event_object_type)
            {
                case 0:     /*MHS ADDR*/
                case 1:     /*DISTINGUISHED_NAME*/
                case 2:     /*TEXT*/
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    break;
                case 3:     /*OCTET_STRING*/
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(atree, hf_ndps_octet_string, tvb, foffset, length, FALSE);
                    foffset += length;
                    foffset += (length%2);
                    break;
                case 4:     /*DIST_NAME_STRING*/
                    foffset = ndps_string(tvb, hf_object_name, atree, foffset);
                    foffset = name_or_id(tvb, atree, foffset);
                    break;
                case 5:     /*RPC_ADDRESS*/
                case 6:     /*QUALIFIED_NAME*/
                    foffset = objectidentifier(tvb, atree, foffset);
                    qualified_name_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                    4, qualified_name_type);
                    foffset += 4;
                    if (qualified_name_type != 0) {
                        if (qualified_name_type == 1) {
                            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                        }
                        else
                        {
                            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                            foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                        }
                    }
                    break;
                default:
                    break;
            }
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = ndps_string(tvb, hf_object_name, atree, foffset);
                foffset = objectidentifier(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 
                4, FALSE);
                foffset += 4;
            }
            foffset = objectidentifier(tvb, atree, foffset);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = objectidentifier(tvb, atree, foffset);
            }
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            break;
        default:
            break;
    }
    return foffset;
}

static int
filteritem(tvbuff_t* tvb, proto_tree *ndps_tree, int foffset)
{
    guint32     filter_op=0;
    guint32     number_of_items=0;
    guint32     i;

    proto_tree_add_item(ndps_tree, hf_ndps_item_filter, tvb, foffset, 4, FALSE);
    filter_op = tvb_get_ntohl(tvb, foffset);
    foffset += 4;
    switch(filter_op)
    {
    case 0:       /* Equality */
    case 2:       /* Greater or Equal */
    case 3:       /* Less or Equal */
    case 5:       /* Subset of */
    case 6:       /* Superset of */
    case 7:       /* Non NULL Set Intersect */
        foffset = objectidentifier(tvb, ndps_tree, foffset);
        number_of_items = tvb_get_ntohl(tvb, foffset);
        proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
        foffset += 4;
        for (i = 1 ; i <= number_of_items; i++ )
        {
            foffset = attribute_value(tvb, ndps_tree, foffset);
        }
        proto_tree_add_item(ndps_tree, hf_ndps_qualifier, tvb, foffset, 4, FALSE);
        foffset += 4;
        break;
    case 1:        /* Substrings */
        foffset = objectidentifier(tvb, ndps_tree, foffset);
        proto_tree_add_item(ndps_tree, hf_ndps_substring_match, tvb, foffset, 4, FALSE);
        foffset += 4;
        foffset = attribute_value(tvb, ndps_tree, foffset); /* initial value */
        number_of_items = tvb_get_ntohl(tvb, foffset);
        proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
        foffset += 4;
        for (i = 1 ; i <= number_of_items; i++ )
        {
            foffset = attribute_value(tvb, ndps_tree, foffset);   /* Any Value Set Option */
        }
        foffset = attribute_value(tvb, ndps_tree, foffset);   /* Final Value */
        break;
    case 4:       /* Present */
        foffset = objectidentifier(tvb, ndps_tree, foffset);
    default:
        break;
    }
    return foffset;
}

static const fragment_items ndps_frag_items = {
	&ett_ndps_segment,
	&ett_ndps_segments,
	&hf_ndps_segments,
	&hf_ndps_segment,
	&hf_ndps_segment_overlap,
	&hf_ndps_segment_overlap_conflict,
	&hf_ndps_segment_multiple_tails,
	&hf_ndps_segment_too_long_segment,
	&hf_ndps_segment_error,
	NULL,
	"segments"
};

static dissector_handle_t ndps_data_handle;

/* NDPS packets come in request/reply pairs. The request packets tell the 
 * Function and Program numbers. The response, unfortunately, only
 * identifies itself via the Exchange ID; you have to know what type of NDPS
 * request the request packet contained in order to successfully parse the 
 * response. A global method for doing this does not exist in ethereal yet
 * (NFS also requires it), so for now the NDPS section will keep its own hash
 * table keeping track of NDPS packets.
 *
 * We construct a conversation specified by the client and server
 * addresses and the connection number; the key representing the unique
 * NDPS request then is composed of the pointer to the conversation
 * structure, cast to a "guint" (which may throw away the upper 32
 * bits of the pointer on a P64 platform, but the low-order 32 bits
 * are more likely to differ between conversations than the upper 32 bits),
 * and the sequence number.
 *
 * The value stored in the hash table is the ncp_req_hash_value pointer. This
 * struct tells us the NDPS Program and Function and gives the NDPS_record pointer.
 */
typedef struct {
	conversation_t	*conversation;
	guint32		    ndps_xport;
} ndps_req_hash_key;

typedef struct {
        guint32             ndps_prog;
        guint32             ndps_func;
        guint32             ndps_frame_num;
} ndps_req_hash_value;

static GHashTable *ndps_req_hash = NULL;
static GMemChunk *ndps_req_hash_keys = NULL;
static GMemChunk *ndps_req_hash_values = NULL;

/* Hash Functions */
gint
ndps_equal(gconstpointer v, gconstpointer v2)
{
	const ndps_req_hash_key	*val1 = (const ndps_req_hash_key*)v;
	const ndps_req_hash_key	*val2 = (const ndps_req_hash_key*)v2;

	if (val1->conversation == val2->conversation &&
	    val1->ndps_xport  == val2->ndps_xport ) {
		return 1;
	}
	return 0;
}

guint
ndps_hash(gconstpointer v)
{
	const ndps_req_hash_key	*ndps_key = (const ndps_req_hash_key*)v;
	return GPOINTER_TO_UINT(ndps_key->conversation) + ndps_key->ndps_xport;
}

/* Initializes the hash table and the mem_chunk area each time a new
 * file is loaded or re-loaded in ethereal */
static void
ndps_init_protocol(void)
{
  	/* fragment */
  	fragment_table_init(&ndps_fragment_table);
  	reassembled_table_init(&ndps_reassembled_table);

	if (ndps_req_hash)
		g_hash_table_destroy(ndps_req_hash);
	if (ndps_req_hash_keys)
		g_mem_chunk_destroy(ndps_req_hash_keys);
	if (ndps_req_hash_values)
		g_mem_chunk_destroy(ndps_req_hash_values);

	ndps_req_hash = g_hash_table_new(ndps_hash, ndps_equal);
	ndps_req_hash_keys = g_mem_chunk_new("ndps_req_hash_keys",
			sizeof(ndps_req_hash_key),
			NDPS_PACKET_INIT_COUNT * sizeof(ndps_req_hash_key),
			G_ALLOC_ONLY);
	ndps_req_hash_values = g_mem_chunk_new("ndps_req_hash_values",
			sizeof(ndps_req_hash_value),
			NDPS_PACKET_INIT_COUNT * sizeof(ndps_req_hash_value),
			G_ALLOC_ONLY);
}

/* After the sequential run, we don't need the ncp_request hash and keys
 * anymore; the lookups have already been done and the vital info
 * saved in the reply-packets' private_data in the frame_data struct. */
static void
ndps_postseq_cleanup(void)
{
	if (ndps_req_hash) {
		/* Destroy the hash, but don't clean up request_condition data. */
		g_hash_table_destroy(ndps_req_hash);
		ndps_req_hash = NULL;
	}
	if (ndps_req_hash_keys) {
		g_mem_chunk_destroy(ndps_req_hash_keys);
		ndps_req_hash_keys = NULL;
	}
	/* Don't free the ncp_req_hash_values, as they're
	 * needed during random-access processing of the proto_tree.*/
}

ndps_req_hash_value*
ndps_hash_insert(conversation_t *conversation, guint32 ndps_xport)
{
	ndps_req_hash_key		*request_key;
	ndps_req_hash_value		*request_value;

	/* Now remember the request, so we can find it if we later
	   a reply to it. */
	request_key = g_mem_chunk_alloc(ndps_req_hash_keys);
	request_key->conversation = conversation;
	request_key->ndps_xport = ndps_xport;

	request_value = g_mem_chunk_alloc(ndps_req_hash_values);
	request_value->ndps_prog = 0;
	request_value->ndps_func = 0;
	request_value->ndps_frame_num = 0;
       
	g_hash_table_insert(ndps_req_hash, request_key, request_value);

	return request_value;
}

/* Returns the ncp_rec*, or NULL if not found. */
ndps_req_hash_value*
ndps_hash_lookup(conversation_t *conversation, guint32 ndps_xport)
{
	ndps_req_hash_key		request_key;

	request_key.conversation = conversation;
	request_key.ndps_xport = ndps_xport;

	return g_hash_table_lookup(ndps_req_hash, &request_key);
}

/* ================================================================= */
/* NDPS                                                               */
/* ================================================================= */

static void
dissect_ndps(tvbuff_t *tvb, packet_info *pinfo, proto_tree *ndps_tree)
{
    guint32     ndps_xid;
    guint32     ndps_prog;
    guint32     ndps_packet_type;
    guint32     ndps_rpc_version;
    int         foffset;
    guint32     ndps_hfname;
    guint32     ndps_func;
    const char  *ndps_program_string;
    const char  *ndps_func_string;


    ndps_packet_type = tvb_get_ntohl(tvb, 8);
    if (ndps_packet_type != 0 && ndps_packet_type != 1) {     /* Packet Type */
        if (check_col(pinfo->cinfo, COL_INFO))
            col_set_str(pinfo->cinfo, COL_INFO, "(Continuation Data)");
        proto_tree_add_text(ndps_tree, tvb, 0, tvb_length_remaining(tvb, 0), "Data - (%d Bytes)", tvb_length_remaining(tvb, 0));
        return;
    }
    foffset = 0;
    proto_tree_add_item(ndps_tree, hf_ndps_record_mark, tvb,
                   foffset, 2, FALSE);
    foffset += 2;
    proto_tree_add_item(ndps_tree, hf_ndps_length, tvb,
                   foffset, 2, FALSE);
    foffset += 2;

    ndps_xid = tvb_get_ntohl(tvb, foffset);
    proto_tree_add_uint(ndps_tree, hf_ndps_xid, tvb, foffset, 4, ndps_xid);
    foffset += 4;
    ndps_packet_type = tvb_get_ntohl(tvb, foffset);
    proto_tree_add_uint(ndps_tree, hf_ndps_packet_type, tvb, foffset, 4, ndps_packet_type);
    foffset += 4;
    if(ndps_packet_type == 0x00000001)          /* Reply packet */
    {
        if (check_col(pinfo->cinfo, COL_INFO))
            col_set_str(pinfo->cinfo, COL_INFO, "R NDPS ");
        proto_tree_add_item(ndps_tree, hf_ndps_rpc_accept, tvb, foffset, 4, FALSE);
        if (tvb_get_ntohl(tvb, foffset)==0) {
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_auth_null, tvb, foffset, 8, FALSE);
            foffset += 8;
        }
        else
        {
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_rpc_rej_stat, tvb, foffset+4, 4, FALSE);
            foffset += 4;
        }
        dissect_ndps_reply(tvb, pinfo, ndps_tree, foffset);
    }
    else
    {
        if (check_col(pinfo->cinfo, COL_INFO))
            col_set_str(pinfo->cinfo, COL_INFO, "C NDPS ");
        ndps_rpc_version = tvb_get_ntohl(tvb, foffset);
        proto_tree_add_item(ndps_tree, hf_ndps_rpc_version, tvb, foffset, 4, FALSE);
        foffset += 4;
        ndps_prog = tvb_get_ntohl(tvb, foffset);
        ndps_program_string = match_strval(ndps_prog, spx_ndps_program_vals);
        if( ndps_program_string != NULL)
        {
            proto_tree_add_item(ndps_tree, hf_spx_ndps_program, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (check_col(pinfo->cinfo, COL_INFO))
            {
                col_append_str(pinfo->cinfo, COL_INFO, (const gchar*) ndps_program_string);
                col_append_str(pinfo->cinfo, COL_INFO, ", ");
            }
            proto_tree_add_item(ndps_tree, hf_spx_ndps_version, tvb, foffset, 4, FALSE);
            foffset += 4;
            ndps_func = tvb_get_ntohl(tvb, foffset);
            switch(ndps_prog)
            {
                case 0x060976:
                    ndps_hfname = hf_spx_ndps_func_print;
                    ndps_func_string = match_strval(ndps_func, spx_ndps_print_func_vals);
                    break;
                case 0x060977:
                    ndps_hfname = hf_spx_ndps_func_broker;
                    ndps_func_string = match_strval(ndps_func, spx_ndps_broker_func_vals);
                    break;
                case 0x060978:
                    ndps_hfname = hf_spx_ndps_func_registry;
                    ndps_func_string = match_strval(ndps_func, spx_ndps_registry_func_vals);
                    break;
                case 0x060979:
                    ndps_hfname = hf_spx_ndps_func_notify;
                    ndps_func_string = match_strval(ndps_func, spx_ndps_notify_func_vals);
                    break;
                case 0x06097a:
                    ndps_hfname = hf_spx_ndps_func_resman;
                    ndps_func_string = match_strval(ndps_func, spx_ndps_resman_func_vals);
                    break;
                case 0x06097b:
                    ndps_hfname = hf_spx_ndps_func_delivery;
                    ndps_func_string = match_strval(ndps_func, spx_ndps_deliver_func_vals);
                    break;
                default:
                    ndps_hfname = 0;
                    ndps_func_string = NULL;
                    break;
            }
            if(ndps_hfname != 0)
            {
                proto_tree_add_item(ndps_tree, ndps_hfname, tvb, foffset, 4, FALSE);
                if (ndps_func_string != NULL) 
                {
                    if (check_col(pinfo->cinfo, COL_INFO))
                        col_append_str(pinfo->cinfo, COL_INFO, (const gchar*) ndps_func_string);

                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_auth_null, tvb, foffset, 16, FALSE);
                    foffset+=16;
                    dissect_ndps_request(tvb, pinfo, ndps_tree, ndps_prog, ndps_func, foffset);
                }
            }
        }
    }
}

static guint
get_ndps_pdu_len(tvbuff_t *tvb, int offset)
{
    guint16 plen;

    /*
     * Get the length of the NDPS packet.
     */
    plen = tvb_get_ntohs(tvb, offset + 2);

    /*
     * That length doesn't include the length of the record mark field
     * or the length field itself; add that in.
     * (XXX - is the field really a 31-bit length with the uppermost bit
     * being a record mark bit?)
     */
    return plen + 4;
}

static void
dissect_ndps_pdu(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    proto_tree	    *ndps_tree = NULL;
    proto_item	    *ti;

    if (check_col(pinfo->cinfo, COL_PROTOCOL))
        col_set_str(pinfo->cinfo, COL_PROTOCOL, "NDPS");

    if (check_col(pinfo->cinfo, COL_INFO))
        col_clear(pinfo->cinfo, COL_INFO);
	
    if (tree) {
        ti = proto_tree_add_item(tree, proto_ndps, tvb, 0, -1, FALSE);
        ndps_tree = proto_item_add_subtree(ti, ett_ndps);
    }
    dissect_ndps(tvb, pinfo, ndps_tree);
}

static void
ndps_defrag(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    guint16         record_mark=0;
    guint16         ndps_length=0;
    guint32         id=0;
    int             len=0;
    tvbuff_t        *next_tvb = NULL;
    fragment_data   *fd_head;

    if (!ndps_defragment) {
        dissect_ndps(tvb, pinfo, tree);
        return;
    }
    record_mark = tvb_get_ntohs(tvb, 0);
    ndps_length = tvb_get_ntohs(tvb, 2);

    if (ndps_length > tvb_length_remaining(tvb, 0) || ndps_fragmented || ndps_length==0) 
    {
        more_fragment = TRUE;
        ndps_fragmented = TRUE;

        /*
         * Fragment
         */
        tid = (pinfo->srcport+pinfo->destport);
        len = tvb_reported_length_remaining(tvb, 0);
        if ((frag_number + tvb_length_remaining(tvb, 0)-save_frag_length)<=10)
        {
            more_fragment = FALSE;
        }
        if (tvb_bytes_exist(tvb, 0, len))
        {
            fd_head = fragment_add_seq_next(tvb, 0, pinfo, tid, ndps_fragment_table, ndps_reassembled_table, len, more_fragment);
            if (fd_head != NULL) 
            {
                if (fd_head->next != NULL) 
                {
                    next_tvb = tvb_new_real_data(fd_head->data,
                        fd_head->len, fd_head->len);
                    tvb_set_child_real_data_tvbuff(tvb,
                        next_tvb);
                    add_new_data_source(pinfo,
                        next_tvb,
                        "Reassembled NDPS");
                    /* Show all fragments. */
                    if (tree) 
                    {
                        show_fragment_seq_tree(fd_head,
                            &ndps_frag_items,
                            tree, pinfo,
                            next_tvb);
                        tid++;
                    }
                    more_fragment = FALSE;
                    save_frag_length = 0;
                    frag_number=0;
                    ndps_fragmented=FALSE;
                } 
                else 
                {
                    next_tvb = tvb_new_subset(tvb, 0, -1, -1);
                }
            }
            else 
            {
                if (save_frag_length == 0) 
                {
                    save_frag_length = ndps_length;                 /* First Fragment */
                    save_frag_seq = tid;
                }
                if ((pinfo->srcport+pinfo->destport) == save_frag_seq) 
                {
                    if (!pinfo->fd->flags.visited) 
                    {
                        frag_number += tvb_length_remaining(tvb, 0);    /* Current offset */
                    }
                    if (check_col(pinfo->cinfo, COL_INFO))
                    {
                      if (more_fragment)
                      {
                        col_append_fstr(pinfo->cinfo, COL_INFO, " [NDPS Fragment]");
                      }
                    }
                }
                next_tvb = NULL;
            }
        }
        else 
        {
            /*
             * Dissect this
             */
            next_tvb = tvb_new_subset(tvb, 0, -1, -1);
        }
        if (next_tvb == NULL)
        {
            if ((pinfo->srcport+pinfo->destport) == save_frag_seq) 
            {
                next_tvb = tvb_new_subset (tvb, 0, -1, -1);
                call_dissector(ndps_data_handle, next_tvb, pinfo, tree);
            }
            else
            {
                dissect_ndps(tvb, pinfo, tree);
            }
        }
        else
        {
            dissect_ndps(next_tvb, pinfo, tree);
        }
    }
    else
    {
        dissect_ndps(tvb, pinfo, tree);
    }
}

static void
dissect_ndps_tcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    tcp_dissect_pdus(tvb, pinfo, tree, ndps_desegment, 4, get_ndps_pdu_len,
	dissect_ndps_pdu);
}


static void
dissect_ndps_ipx(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    proto_tree	    *ndps_tree = NULL;
    proto_item	    *ti;

    if (check_col(pinfo->cinfo, COL_PROTOCOL))
        col_set_str(pinfo->cinfo, COL_PROTOCOL, "NDPS");

    if (check_col(pinfo->cinfo, COL_INFO))
        col_clear(pinfo->cinfo, COL_INFO);
	
    if (tree) {
        ti = proto_tree_add_item(tree, proto_ndps, tvb, 0, -1, FALSE);
        ndps_tree = proto_item_add_subtree(ti, ett_ndps);
    }
    ndps_defrag(tvb, pinfo, ndps_tree);
}

static void
dissect_ndps_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *ndps_tree, guint32 ndps_prog, guint32 ndps_func, int foffset)
{
    ndps_req_hash_value	*request_value = NULL;
    conversation_t      *conversation;
    guint32             i=0;
    guint32             j=0;
    guint32             name_len=0;
    guint32             field_len=0;
    guint32             cred_type=0;
    guint32             resource_type=0;
    guint32             filter_type=0;
    guint32             print_type=0;
    guint32             qualified_name_type=0;
    guint32             data_type=0;
    guint32             length=0;
    guint32             number_of_items=0;
    guint32             number_of_items2=0;
    proto_tree          *atree;
    proto_item          *aitem;
    proto_tree          *btree;
    proto_item          *bitem;
    proto_tree          *ctree;
    proto_item          *citem;

    if (!pinfo->fd->flags.visited) 
    {

        /* This is the first time we've looked at this packet.
        Keep track of the Program and connection whence the request
        came, and the address and connection to which the request
        is being sent, so that we can match up calls with replies.
        (We don't include the sequence number, as we may want
        to have all packets over the same connection treated
        as being part of a single conversation so that we can
        let the user select that conversation to be displayed.) */
        
        conversation = find_conversation(&pinfo->src, &pinfo->dst,
            PT_NCP, (guint32) pinfo->srcport, (guint32) pinfo->srcport, 0);

        if (conversation == NULL) 
            {
            /* It's not part of any conversation - create a new one. */
            conversation = conversation_new(&pinfo->src, &pinfo->dst,
                PT_NCP, (guint32) pinfo->srcport, (guint32) pinfo->srcport, 0);
        }

        request_value = ndps_hash_insert(conversation, (guint32) pinfo->srcport);
        request_value->ndps_prog = ndps_prog;
        request_value->ndps_func = ndps_func;
        request_value->ndps_frame_num = pinfo->fd->num;
    }
    switch(ndps_prog)
    {
    case 0x060976:  /* Print */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind PSM */
            proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
            4, FALSE);
            foffset += 4;
            name_len = tvb_get_ntohl(tvb, foffset);
            foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
            /*if(name_len == 0)
            {
                foffset += 2;
            }
            foffset += 2;*/
            proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
            2, FALSE);
            foffset += 2;
            foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
            break;
        case 0x00000002:    /* Bind PA */
            /* Start of credentials */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_cred_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_password, tvb, foffset, length, FALSE);
                    foffset += length;
                }
                break;
            case 1:
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_certified, tvb, foffset, length, FALSE);
                foffset += length;
                break;
            case 2:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                break;
            case 3:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                break;
            case 4:
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                foffset += 8;   /* Don't know what these 8 bytes signify */
                proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
                4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                break;
            default:
                break;
            }
            /* End of credentials */
            proto_tree_add_item(ndps_tree, hf_ndps_retrieve_restrictions, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_bind_security, tvb, foffset, length, FALSE);
            }
            /* Start of QualifiedName */
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(ndps_tree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, ndps_tree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                }
            }
            /* End of QualifiedName */
            break;
        case 0x00000003:    /* Unbind */
            proto_tree_add_item(ndps_tree, hf_ndps_object, tvb, foffset, 
            4, FALSE);
            break;
        case 0x00000004:    /* Print */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_print_arg, tvb, foffset, 4, FALSE);
            print_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (print_type) 
            {
            case 0:     /* Create Job */
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_sub_complete, tvb, foffset, 4, FALSE);
                foffset += 4;
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Transfer Method");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                    number_of_items2 = tvb_get_ntohl(tvb, foffset);
                    bitem = proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                    btree = proto_item_add_subtree(bitem, ett_ndps);
                    foffset += 4;
                    for (j = 1 ; j <= number_of_items2; j++ )
                    {
                        foffset = attribute_value(tvb, btree, foffset);
                    }
                    proto_tree_add_item(btree, hf_ndps_qualifier, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Document Content");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                foffset += align_4(tvb, foffset);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(atree, hf_doc_content, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    if (tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        if (tvb_get_ntohl(tvb, foffset) > tvb_length_remaining(tvb, foffset)) /* Segmented Data */
                        {
                            proto_tree_add_item(atree, hf_ndps_data, tvb, foffset, tvb_length_remaining(tvb, foffset), FALSE);
                            return;
                        }
                        proto_tree_add_item(atree, hf_ndps_included_doc, tvb, foffset+4, tvb_get_ntohl(tvb, foffset), FALSE);
                        foffset += tvb_get_ntohl(tvb, foffset)+4;
                        foffset += (length%2);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_ref_name, atree, foffset);
                        foffset = name_or_id(tvb, atree, foffset);
                    }
                }
                foffset += 4;
                if (align_4(tvb, foffset)>0) {
                    foffset += align_4(tvb, foffset);
                    foffset += 2;
                }
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Document Type");
                number_of_items = tvb_get_ntohl(tvb, foffset);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = objectidentifier(tvb, atree, foffset);
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    number_of_items2 = tvb_get_ntohl(tvb, foffset);
                    bitem = proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                    btree = proto_item_add_subtree(bitem, ett_ndps);
                    foffset += 4;
                    for (j = 1 ; j <= number_of_items2; j++ )
                    {
                        foffset = attribute_value(tvb, btree, foffset);
                    }
                    proto_tree_add_item(btree, hf_ndps_qualifier, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Document Attributes");
                number_of_items = tvb_get_ntohl(tvb, foffset);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, atree, foffset);  /* Document Attributes */
                }
                break;
            case 1:     /* Add Job */
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_sub_complete, tvb, foffset, 4, FALSE);
                foffset += 4;
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Transfer Method");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = objectidentifier(tvb, atree, foffset); /* Transfer Method */
                }
                proto_tree_add_item(ndps_tree, hf_doc_content, tvb, foffset, 4, FALSE);
                foffset += 4;
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Document Type");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = objectidentifier(tvb, atree, foffset); /* Document Type */
                }
                foffset += align_4(tvb, foffset);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Document Attributes");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, atree, foffset);  /* Document Attributes */
                }
                break;
            case 2:     /* Close Job */
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            break;
        case 0x00000005:    /* Modify Job */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            proto_tree_add_item(ndps_tree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_document_number, tvb, foffset, 4, FALSE);
            foffset += 4;
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job Modifications");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Job Modifications */
            }
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Document Modifications");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Document Modifications */
            }
            break;
        case 0x00000006:    /* Cancel Job */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            proto_tree_add_item(ndps_tree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_document_number, tvb, foffset, 4, FALSE);
            foffset += 4;
            number_of_items = tvb_get_ntohl(tvb, foffset);
            /* Start of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Cancel Message");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Retention Period");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_status_flags, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, 4, FALSE);
            foffset += 4;
             
            break;
        case 0x00000007:    /* List Object Attributes */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_attrs_arg, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0) /* Continuation */
            {
                length = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(ndps_tree, hf_ndps_context, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
                proto_tree_add_item(ndps_tree, hf_ndps_abort_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, atree, foffset);
                }
            }
            else                                  /* Specification */
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                foffset += 4;
                foffset += align_4(tvb, foffset);
                proto_tree_add_item(ndps_tree, hf_ndps_scope, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)!=0)    /* Scope Does not equal 0 */
                {
                    number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPSelector */
                    aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Selector Option");
                    atree = proto_item_add_subtree(aitem, ett_ndps);
                    proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    for (i = 1 ; i <= number_of_items; i++ )
                    {
                        foffset = objectidentification(tvb, atree, foffset);
                    }
                    foffset += align_4(tvb, foffset);
                    /*foffset += 4;*/
                    proto_tree_add_item(ndps_tree, hf_ndps_filter, tvb, foffset, 4, FALSE);
                    filter_type = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    /*if (filter_type == 0 || filter_type == 3 ) 
                    {
                        foffset = filteritem(tvb, ndps_tree, foffset);
                    }
                    else
                    {
                        aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                        atree = proto_item_add_subtree(aitem, ett_ndps);
                        number_of_items = tvb_get_ntohl(tvb, foffset);
                        foffset += 4;
                        for (i = 1 ; i <= number_of_items; i++ )
                        {
                            foffset = filteritem(tvb, ndps_tree, foffset);
                        }
                    }*/
                    proto_tree_add_item(ndps_tree, hf_ndps_time_limit, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_count_limit, tvb, foffset, 4, FALSE);
                    foffset += 4; /* End of NWDPSelector  */
                }
                foffset += 4;   /* Don't know what this is */
                number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPObjectIdentifierSet */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Requested Attributes");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                } /* End of NWDPObjectIdentifierSet */
                proto_tree_add_item(ndps_tree, hf_ndps_operator, tvb, foffset, 4, FALSE);
                foffset += 4;
                number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, atree, foffset);
                } /* NWDPCommonArguments */
            }
            break;
        case 0x00000008:    /* Promote Job */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPPrtContainedObjectId */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPPrtContainedObjectId */
            /* Start of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of nameorid */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000009:    /* Interrupt */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_interrupt_job_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0)
            {
                /* Start of NWDPPrtContainedObjectId */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of NWDPPrtContainedObjectId */
            }
            else
            {
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            }
            /* Start of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Interrupt Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of nameorid */
            /* Start of NWDPPrtContainedObjectId */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Interrupting Job");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPPrtContainedObjectId */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000000a:    /* Pause */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_pause_job_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0)
            {
                /* Start of NWDPPrtContainedObjectId */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of NWDPPrtContainedObjectId */
            }
            else
            {
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            }
            /* Start of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Pause Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of nameorid */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000000b:    /* Resume */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPPrtContainedObjectId */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPPrtContainedObjectId */
            /* Start of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Resume Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of nameorid */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000000c:    /* Clean */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of nameorid */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Clean Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of nameorid */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000000d:    /* Create */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentification(tvb, atree, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_force, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Reference Object Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentification(tvb, atree, foffset);
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Attribute");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Object Attribute Set */
            }
            /* End of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000000e:    /* Delete */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentification(tvb, atree, foffset);
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000000f:    /* Disable PA */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Disable PA Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000010:    /* Enable PA */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Enable PA Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000011:    /* Resubmit Jobs */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(ndps_tree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, ndps_tree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                }
            }
            /* End of QualifiedName */
            foffset = address_item(tvb, ndps_tree, foffset);
            proto_tree_add_item(ndps_tree, hf_resubmit_op_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of ResubmitJob Set */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Resubmit Job");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of NWDPPrtContainedObjectId */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Job ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = ndps_string(tvb, hf_ndps_pa_name, btree, foffset);
                proto_tree_add_item(btree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of NWDPPrtContainedObjectId */
                proto_tree_add_item(atree, hf_ndps_document_number, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of AttributeSet */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Job Attributes");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                proto_tree_add_item(btree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, btree, foffset);  /* Object Attribute Set */
                }
                /* End of AttributeSet */
                /* Start of AttributeSet */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Document Attributes");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                proto_tree_add_item(btree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, btree, foffset);  /* Object Attribute Set */
                }
                /* End of AttributeSet */
            } /* End of ResubmitJob Set */
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Resubmit Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000012:    /* Set */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentification(tvb, atree, foffset);
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Attribute Modifications");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Object Attribute Set */
            }
            /* End of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000013:    /* Shutdown PA */
        case 0x0000001e:    /* Shutdown PSM */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_shutdown_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Shutdown Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000014:    /* Startup PA */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Startup Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000015:    /* Reorder Job */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPPrtContainedObjectId */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job Identification");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPPrtContainedObjectId */
            /* Start of NWDPPrtContainedObjectId */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Reference Job ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPPrtContainedObjectId */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000016:    /* Pause PA */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Pause Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000017:    /* Resume PA */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Resume Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000018:    /* Transfer Data */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_get_status_flag, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_data, tvb, foffset+4, tvb_get_ntohl(tvb, foffset), FALSE);
            break;
        case 0x00000019:    /* Device Control */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of Object Identifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Operation ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of Object Identifier */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x0000001a:    /* Add Event Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of Eventhandling */
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_persistence, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Consumer Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
            foffset += length;
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Delivery Address");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset += address_item(tvb, atree, foffset);
            }
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = objectidentifier(tvb, atree, foffset);
                foffset = objectidentification(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items2 = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items2);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items2; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
            }
            /* End of Eventhandling */
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Account");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            break;
        case 0x0000001b:    /* Remove Event Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x0000001c:    /* Modify Event Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_supplier_flag, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
            }
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_language_flag, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                proto_tree_add_item(atree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_method_flag, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                /* Start of NameorID */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Method ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = name_or_id(tvb, btree, foffset);
                /* End of NameorID */
            }
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_delivery_address_flag, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                foffset = print_address(tvb, atree, foffset);
            }
            /* Start of EventObjectSet */
            length = tvb_get_ntohl(tvb, foffset);   /* Len of record */
            if (length > 0) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of objectidentifier */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Class ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                /* End of objectidentifier */
                /* Start of objectidentification */
                bitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                /* End of objectidentification */
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of ObjectItem */
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
                /* End of ObjectItem */
            }
            /* End of EventObjectSet */
            break;
        case 0x0000001d:    /* List Event Profiles */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_list_profiles_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0)   /* Spec */
            {
                /* Start of QualifiedName */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier Alias");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                qualified_name_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                4, qualified_name_type);
                foffset += 4;
                if (qualified_name_type != 0) {
                    if (qualified_name_type == 1) {
                        foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                        foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    }
                }
                /* End of QualifiedName */
                proto_tree_add_item(ndps_tree, hf_ndps_list_profiles_choice_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)==0)   /* Choice */
                {
                    /* Start of CardinalSeq */
                    proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                    number_of_items = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    for (i = 1 ; i <= number_of_items; i++ )
                    {
                        length = tvb_get_ntohl(tvb, foffset);
                        foffset += 4;
                        proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                        foffset += length;
                        foffset += (length%2);
                    }
                    /* End of CardinalSeq */
                }
                else
                {
                    /* Start of QualifiedName */
                    aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Consumer");
                    atree = proto_item_add_subtree(aitem, ett_ndps);
                    qualified_name_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                    4, qualified_name_type);
                    foffset += 4;
                    if (qualified_name_type != 0) {
                        if (qualified_name_type == 1) {
                            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                        }
                        else
                        {
                            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                            foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                        }
                    }
                    /* End of QualifiedName */
                    /* Start of NameorID */
                    aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
                    atree = proto_item_add_subtree(aitem, ett_ndps);
                    foffset = name_or_id(tvb, atree, foffset);
                    /* End of NameorID */
                    proto_tree_add_item(atree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                proto_tree_add_item(ndps_tree, hf_ndps_list_profiles_result_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of integeroption */
                proto_tree_add_item(ndps_tree, hf_ndps_integer_type_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)!=0) 
                {
                    proto_tree_add_item(ndps_tree, hf_ndps_integer_type_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                /* End of integeroption */
            }
            else                                    /* Cont */
            {
                length = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(ndps_tree, hf_ndps_context, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
                proto_tree_add_item(ndps_tree, hf_ndps_abort_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            break;
        case 0x0000001f:    /* Cancel PSM Shutdown */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Cancel Shutdown Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000020:    /* Set Printer DS Information */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_ds_info_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_printer_name, ndps_tree, foffset);
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "DS Object Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            break;
        case 0x00000021:    /* Clean User Jobs */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Clean Message Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of NWDPCommonArguments */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Common Arguments");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);
            } /* NWDPCommonArguments */
            break;
        case 0x00000022:    /* Map GUID to NDS Name */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_guid, tvb, foffset, length, FALSE);
            foffset += length;
            break;
        default:
            break;
        }
        break;
    case 0x060977:  /* Broker */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind */
            /* Start of credentials */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_cred_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_password, tvb, foffset, length, FALSE);
                    foffset += length;
                }
                break;
            case 1:
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_certified, tvb, foffset, length, FALSE);
                foffset += length;
                break;
            case 2:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                break;
            case 3:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                break;
            case 4:
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                foffset += 8;   /* Don't know what these 8 bytes signify */
                proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
                4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                break;
            default:
                break;
            }
            /* End of credentials */
            proto_tree_add_item(ndps_tree, hf_ndps_retrieve_restrictions, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_bind_security, tvb, foffset, length, FALSE);
            }
            break;
        case 0x00000002:    /* Unbind */
            break;
        case 0x00000003:    /* List Services */
            proto_tree_add_item(ndps_tree, hf_ndps_list_services_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x00000004:    /* Enable Service */
            proto_tree_add_item(ndps_tree, hf_ndps_service_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Parameters");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_item_bytes, tvb, foffset, length, FALSE);
                foffset += length;
            } 
            break;
        case 0x00000005:    /* Disable Service */
            proto_tree_add_item(ndps_tree, hf_ndps_list_services_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x00000006:    /* Down Broker */
        case 0x00000007:    /* Get Broker NDS Object Name */
        case 0x00000008:    /* Get Broker Session Information */
        default:
            break;
        }
        break;
    case 0x060978:  /* Registry */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind */
            /* Start of credentials */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_cred_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_password, tvb, foffset, length, FALSE);
                    foffset += length;
                }
                break;
            case 1:
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_certified, tvb, foffset, length, FALSE);
                foffset += length;
                break;
            case 2:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                break;
            case 3:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                break;
            case 4:
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                foffset += 8;   /* Don't know what these 8 bytes signify */
                proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
                4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                break;
            default:
                break;
            }
            /* End of credentials */
            proto_tree_add_item(ndps_tree, hf_ndps_retrieve_restrictions, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_bind_security, tvb, foffset, length, FALSE);
            }
            break;
        case 0x00000002:    /* Unbind */
            break;
        case 0x00000003:    /* Register Server */
            /* Start of Server Entry */
            foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_server_type, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            foffset = print_address(tvb, atree, foffset);
            bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Server Info");
            btree = proto_item_add_subtree(bitem, ett_ndps);
            number_of_items2 = tvb_get_ntohl(tvb, foffset); 
            proto_tree_add_item(btree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (j = 1 ; j <= number_of_items2; j++ )
            {
                data_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(btree, hf_ndps_data_item_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                switch (data_type) 
                {
                case 0:   /* Int8 */
                    proto_tree_add_item(btree, hf_info_int, tvb, foffset, 1, FALSE);
                    foffset++;
                    break;
                case 1:   /* Int16 */
                    proto_tree_add_item(btree, hf_info_int16, tvb, foffset, 2, FALSE);
                    foffset += 2;
                    break;
                case 2:   /* Int32 */
                    proto_tree_add_item(btree, hf_info_int32, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    break;
                case 3:   /* Boolean */
                    proto_tree_add_item(btree, hf_info_boolean, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    break;
                case 4:   /* String */
                case 5:   /* Bytes */
                    foffset = ndps_string(tvb, hf_info_string, btree, foffset);
                    break;
                    /*length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(btree, hf_info_bytes, tvb, foffset, length, FALSE);
                    foffset += length;
                    foffset += (length%4);*/
                    break;
                default:
                    break;
                }
            }
            /* End of Server Entry */
            break;
        case 0x00000004:    /* Deregister Server */
        case 0x00000006:    /* Deregister Registry */
        case 0x0000000b:    /* Get Registry NDS Object Name */
        case 0x0000000c:    /* Get Registry Session Information */
            /* NoOp */
            break;
        case 0x00000005:    /* Register Registry */
            foffset = ndps_string(tvb, hf_ndps_registry_name, ndps_tree, foffset);
            foffset = print_address(tvb, ndps_tree, foffset);
            break;
        case 0x00000007:    /* Registry Update */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Add");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of Server Entry */
                foffset = ndps_string(tvb, hf_ndps_server_name, atree, foffset);
                bitem = proto_tree_add_item(atree, hf_ndps_server_type, tvb, foffset, 4, FALSE);
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset += 4;
                foffset = print_address(tvb, btree, foffset);
                citem = proto_tree_add_text(btree, tvb, foffset, 0, "Server Info");
                ctree = proto_item_add_subtree(citem, ett_ndps);
                number_of_items2 = tvb_get_ntohl(tvb, foffset); 
                proto_tree_add_item(ctree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (j = 1 ; j <= number_of_items2; j++ )
                {
                    data_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_item(ctree, hf_ndps_data_item_type, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    switch (data_type) 
                    {
                    case 0:   /* Int8 */
                        proto_tree_add_item(ctree, hf_info_int, tvb, foffset, 1, FALSE);
                        foffset++;
                        break;
                    case 1:   /* Int16 */
                        proto_tree_add_item(ctree, hf_info_int16, tvb, foffset, 2, FALSE);
                        foffset += 2;
                        break;
                    case 2:   /* Int32 */
                        proto_tree_add_item(ctree, hf_info_int32, tvb, foffset, 4, FALSE);
                        foffset += 4;
                        break;
                    case 3:   /* Boolean */
                        proto_tree_add_item(ctree, hf_info_boolean, tvb, foffset, 4, FALSE);
                        foffset += 4;
                        break;
                    case 4:   /* String */
                    case 5:   /* Bytes */
                        foffset = ndps_string(tvb, hf_info_string, ctree, foffset);
                        break;
                        /*length = tvb_get_ntohl(tvb, foffset);
                        foffset += 4;
                        proto_tree_add_item(ctree, hf_info_bytes, tvb, foffset, length, FALSE);
                        foffset += length;
                        foffset += (length%4);*/
                        break;
                    default:
                        break;
                    }
                }
                /* End of Server Entry */
            }
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Remove");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of Server Entry */
                foffset = ndps_string(tvb, hf_ndps_server_name, atree, foffset);
                bitem = proto_tree_add_item(atree, hf_ndps_server_type, tvb, foffset, 4, FALSE);
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset += 4;
                foffset = print_address(tvb, btree, foffset);
                citem = proto_tree_add_text(btree, tvb, foffset, 0, "Server Info");
                ctree = proto_item_add_subtree(citem, ett_ndps);
                number_of_items2 = tvb_get_ntohl(tvb, foffset); 
                proto_tree_add_item(ctree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (j = 1 ; j <= number_of_items2; j++ )
                {
                    data_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_item(ctree, hf_ndps_data_item_type, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    switch (data_type) 
                    {
                    case 0:   /* Int8 */
                        proto_tree_add_item(ctree, hf_info_int, tvb, foffset, 1, FALSE);
                        foffset++;
                        break;
                    case 1:   /* Int16 */
                        proto_tree_add_item(ctree, hf_info_int16, tvb, foffset, 2, FALSE);
                        foffset += 2;
                        break;
                    case 2:   /* Int32 */
                        proto_tree_add_item(ctree, hf_info_int32, tvb, foffset, 4, FALSE);
                        foffset += 4;
                        break;
                    case 3:   /* Boolean */
                        proto_tree_add_item(ctree, hf_info_boolean, tvb, foffset, 4, FALSE);
                        foffset += 4;
                        break;
                    case 4:   /* String */
                    case 5:   /* Bytes */
                        foffset = ndps_string(tvb, hf_info_string, ctree, foffset);
                        break;
                        /*length = tvb_get_ntohl(tvb, foffset);
                        foffset += 4;
                        proto_tree_add_item(ctree, hf_info_bytes, tvb, foffset, length, FALSE);
                        foffset += length;
                        foffset += (length%4);*/
                        break;
                    default:
                        break;
                    }
                }
                /* End of Server Entry */
            }
            break;
        case 0x00000008:    /* List Local Servers */
        case 0x00000009:    /* List Servers */
        case 0x0000000a:    /* List Known Registries */
            proto_tree_add_item(ndps_tree, hf_ndps_list_local_servers_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0) 
            {
                /* Start of integeroption */
                proto_tree_add_item(ndps_tree, hf_ndps_integer_type_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)!=0) 
                {
                    proto_tree_add_item(ndps_tree, hf_ndps_integer_type_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                /* End of integeroption */
            }
            else
            {
                length = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(ndps_tree, hf_ndps_context, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
                proto_tree_add_item(ndps_tree, hf_ndps_abort_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            break;
        default:
            break;
        }
        break;
    case 0x060979:  /* Notify */
        switch(ndps_func)
        {
        case 0x00000001:    /* Notify Bind */
            /* Start of credentials */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_cred_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_password, tvb, foffset, length, FALSE);
                    foffset += length;
                }
                break;
            case 1:
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_certified, tvb, foffset, length, FALSE);
                foffset += length;
                break;
            case 2:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                break;
            case 3:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                break;
            case 4:
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                foffset += 8;   /* Don't know what these 8 bytes signify */
                proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
                4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                break;
            default:
                break;
            }
            /* End of credentials */
            proto_tree_add_item(ndps_tree, hf_ndps_retrieve_restrictions, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_bind_security, tvb, foffset, length, FALSE);
            }
            break;
        case 0x00000002:    /* Notify Unbind */
        case 0x0000000a:    /* List Supported Languages */
        case 0x00000010:    /* Get Notify NDS Object Name */
        case 0x00000011:    /* Get Notify Session Information */
            /* NoOp */
            break;
        case 0x00000003:    /* Register Supplier */
            foffset = ndps_string(tvb, hf_ndps_supplier_name, ndps_tree, foffset);
            /* Start of QualifiedName Set*/
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of QualifiedName */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Supplier Alias");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                qualified_name_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(btree, hf_ndps_qualified_name, tvb, foffset, 
                4, qualified_name_type);
                foffset += 4;
                if (qualified_name_type != 0) {
                    if (qualified_name_type == 1) {
                        foffset = ndps_string(tvb, hf_printer_name, btree, foffset);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_pa_name, btree, foffset);
                        foffset = ndps_string(tvb, hf_ndps_tree, btree, foffset);
                    }
                }
                /* End of QualifiedName */
            }
            /* End of QualifiedName Set*/
            break;
        case 0x00000004:    /* Deregister Supplier */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x00000005:    /* Add Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier Alias");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            /* Start of Eventhandling */
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_persistence, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Consumer Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
            foffset += length;
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Delivery Address");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset += address_item(tvb, atree, foffset);
            }
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = objectidentifier(tvb, atree, foffset);
                foffset = objectidentification(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items2 = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items2);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items2; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
            }
            /* End of Eventhandling */
            break;
        case 0x00000006:    /* Remove Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x00000007:    /* Modify Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_supplier_flag, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
            }
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_language_flag, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                proto_tree_add_item(atree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_method_flag, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                /* Start of NameorID */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Method ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = name_or_id(tvb, btree, foffset);
                /* End of NameorID */
            }
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_delivery_address_flag, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==TRUE) 
            {
                foffset = print_address(tvb, atree, foffset);
            }
            /* Start of EventObjectSet */
            length = tvb_get_ntohl(tvb, foffset);   /* Len of record */
            if (length > 0) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of objectidentifier */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Class ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                /* End of objectidentifier */
                /* Start of objectidentification */
                bitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                /* End of objectidentification */
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of ObjectItem */
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
                /* End of ObjectItem */
            }
            /* End of EventObjectSet */
            break;
        case 0x00000008:    /* List Profiles */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_list_profiles_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0)   /* Spec */
            {
                /* Start of QualifiedName */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier Alias");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                qualified_name_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                4, qualified_name_type);
                foffset += 4;
                if (qualified_name_type != 0) {
                    if (qualified_name_type == 1) {
                        foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                        foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    }
                }
                /* End of QualifiedName */
                proto_tree_add_item(ndps_tree, hf_ndps_list_profiles_choice_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)==0)   /* Choice */
                {
                    /* Start of CardinalSeq */
                    proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                    number_of_items = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    for (i = 1 ; i <= number_of_items; i++ )
                    {
                        length = tvb_get_ntohl(tvb, foffset);
                        foffset += 4;
                        proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                        foffset += length;
                        foffset += (length%2);
                    }
                    /* End of CardinalSeq */
                }
                else
                {
                    /* Start of QualifiedName */
                    aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Consumer");
                    atree = proto_item_add_subtree(aitem, ett_ndps);
                    qualified_name_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                    4, qualified_name_type);
                    foffset += 4;
                    if (qualified_name_type != 0) {
                        if (qualified_name_type == 1) {
                            foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                        }
                        else
                        {
                            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                            foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                        }
                    }
                    /* End of QualifiedName */
                    /* Start of NameorID */
                    aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
                    atree = proto_item_add_subtree(aitem, ett_ndps);
                    foffset = name_or_id(tvb, atree, foffset);
                    /* End of NameorID */
                    proto_tree_add_item(atree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                proto_tree_add_item(ndps_tree, hf_ndps_list_profiles_result_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of integeroption */
                proto_tree_add_item(ndps_tree, hf_ndps_integer_type_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)!=0) 
                {
                    proto_tree_add_item(ndps_tree, hf_ndps_integer_type_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                /* End of integeroption */
            }
            else                                    /* Cont */
            {
                length = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(ndps_tree, hf_ndps_context, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
                proto_tree_add_item(ndps_tree, hf_ndps_abort_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            break;
        case 0x00000009:    /* Report Event */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of ReportEventItemSet */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Items");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of ReportEventItem */
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Containing Class");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Containing Object");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Filter Class");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Object Class");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Event Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                /* Start of AttributeSet */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Attribute Modifications");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                proto_tree_add_item(btree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, btree, foffset);  /* Object Attribute Set */
                }
                /* End of AttributeSet */
                foffset = ndps_string(tvb, hf_ndps_message, atree, foffset);
                proto_tree_add_item(atree, hf_time, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of ReportEventItem */
            }
            /* End of ReportEventItemSet */
            break;
        case 0x0000000b:    /* Report Notification */
            /* Start of DestinationSet */
            proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of Destination */
                /* Start of NameorID */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = name_or_id(tvb, atree, foffset);
                /* End of NameorID */
                /* Start of NotifyDeliveryAddr */
                proto_tree_add_item(atree, hf_address_len, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = print_address(tvb, atree, foffset);
                /* End of NotifyDeliveryAddr */
                /* End of Destination */
            }
            /* End of DestinationSet */
            foffset = ndps_string(tvb, hf_ndps_supplier_name, ndps_tree, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Containing Class");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Containing Object");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentification(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Filter Class");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentification(tvb, atree, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Attribute");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  
            }
            /* End of AttributeSet */
            foffset = ndps_string(tvb, hf_ndps_message, ndps_tree, foffset);
            proto_tree_add_item(ndps_tree, hf_time, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Account");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            break;
        case 0x0000000c:    /* Add Delivery Method */
            foffset = ndps_string(tvb, hf_ndps_file_name, ndps_tree, foffset);
            break;
        case 0x0000000d:    /* Remove Delivery Method */
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            break;
        case 0x0000000e:    /* List Delivery Methods */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_delivery_method_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:        /* Specification */
                /* Start of integeroption */
                proto_tree_add_item(ndps_tree, hf_ndps_integer_type_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                if (tvb_get_ntohl(tvb, foffset-4)!=0) 
                {
                    proto_tree_add_item(ndps_tree, hf_ndps_integer_type_value, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                /* End of integeroption */
                proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            case 1:       /* Continuation */
                length = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_item(ndps_tree, hf_ndps_context, tvb, foffset, length, FALSE);
                foffset += length;
                foffset += (length%2);
                proto_tree_add_item(ndps_tree, hf_ndps_abort_flag, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            break;
        case 0x0000000f:    /* Get Delivery Method Information */
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        default:
            break;
        }
        break;
    case 0x06097a:  /* Resman */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind */
            /* Start of credentials */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_cred_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_password, tvb, foffset, length, FALSE);
                    foffset += length;
                }
                break;
            case 1:
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_certified, tvb, foffset, length, FALSE);
                foffset += length;
                break;
            case 2:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                break;
            case 3:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                break;
            case 4:
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                foffset += 8;   /* Don't know what these 8 bytes signify */
                proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
                4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                break;
            default:
                break;
            }
            /* End of credentials */
            proto_tree_add_item(ndps_tree, hf_ndps_retrieve_restrictions, tvb, foffset, 4, FALSE);
            foffset += 4;
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_bind_security, tvb, foffset, length, FALSE);
            }
            break;
        case 0x00000002:    /* Unbind */
        case 0x00000008:    /* Get Resource Manager NDS Object Name */
        case 0x00000009:    /* Get Resource Manager Session Information */
            /* NoOp */
            break;
        case 0x00000003:    /* Add Resource File */
            proto_tree_add_item(ndps_tree, hf_packet_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_last_packet_flag, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_file_timestamp, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPResAddInputData */
            proto_tree_add_item(ndps_tree, hf_res_type, tvb, foffset, 4, FALSE);
            resource_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (resource_type) 
            {
            case 0:     /* Print Drivers */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 1:     /* Printer Definitions */
                foffset = ndps_string(tvb, hf_ndps_vendor_dir, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 2:     /* Banner Page Files */
                foffset = ndps_string(tvb, hf_ndps_banner_name, ndps_tree, foffset);
                break;
            case 3:     /* Font Types */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_font_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 4:     /* Generic Files/ Archive */
            case 5:     /* Printer Driver Archive */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_archive_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            /* End of NWDPResAddInputData */
            proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items=tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length=tvb_get_ntohl(tvb, foffset);
                if(tvb_length_remaining(tvb, foffset) < length)
                {
                    return;
                }
                proto_tree_add_item(ndps_tree, hf_ndps_item_ptr, tvb, foffset, length, FALSE);
                foffset += length;
            }
            break;
        case 0x00000004:    /* Delete Resource File */
            /* Start of NWDPResAddInputData */
            proto_tree_add_item(ndps_tree, hf_res_type, tvb, foffset, 4, FALSE);
            resource_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (resource_type) 
            {
            case 0:     /* Print Drivers */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 1:     /* Printer Definitions */
                foffset = ndps_string(tvb, hf_ndps_vendor_dir, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 2:     /* Banner Page Files */
                foffset = ndps_string(tvb, hf_ndps_banner_name, ndps_tree, foffset);
                break;
            case 3:     /* Font Types */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_font_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 4:     /* Generic Files/ Archive */
            case 5:     /* Printer Driver Archive */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_archive_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            /* End of NWDPResAddInputData */
            break;
        case 0x00000005:    /* List Resources */
            proto_tree_add_item(ndps_tree, hf_ndps_max_items, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_status_flags, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_resource_list_type, tvb, foffset, 4, FALSE);
            resource_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (resource_type) 
            {
            case 0:     /* Print Drivers */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            case 1:     /* Printer Definitions */
            case 2:     /* Printer Definitions Short */
                foffset = ndps_string(tvb, hf_ndps_vendor_dir, ndps_tree, foffset);
                break;
            case 3:     /* Banner Page Files */
                proto_tree_add_item(ndps_tree, hf_banner_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            case 4:     /* Font Types */
                proto_tree_add_item(ndps_tree, hf_font_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            case 5:     /* Printer Driver Files */
            case 12:    /* Printer Driver Files 2 */
            case 9:     /* Generic Files */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_printer_type, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_printer_manuf, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_inf_file_name, ndps_tree, foffset);
                field_len = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_printer_id, tvb, foffset, field_len, FALSE);
                break;
            case 6:     /* Printer Definition File */
            case 10:    /* Printer Definition File 2 */
                foffset = ndps_string(tvb, hf_ndps_vendor_dir, ndps_tree, foffset);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_printer_type, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_printer_manuf, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_inf_file_name, ndps_tree, foffset);
                field_len = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_printer_id, tvb, foffset, field_len, FALSE);
                break;
            case 7:     /* Font Files */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_font_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_font_name, ndps_tree, foffset);
                break;
            case 8:     /* Generic Type */
            case 11:    /* Printer Driver Types 2 */
            case 13:    /* Printer Driver Types Archive */
                foffset = ndps_string(tvb, hf_ndps_printer_manuf, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_printer_type, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_inf_file_name, ndps_tree, foffset);
                break;
            case 14:    /* Languages Available */
                break;
            default:
                break;
            }
            break;
        case 0x00000006:    /* Get Resource File */
            proto_tree_add_item(ndps_tree, hf_get_status_flag, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_res_type, tvb, foffset, 4, FALSE);
            resource_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (resource_type) 
            {
            case 0:     /* Print Drivers */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 1:     /* Printer Definitions */
                foffset = ndps_string(tvb, hf_ndps_vendor_dir, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 2:     /* Banner Page Files */
                foffset = ndps_string(tvb, hf_ndps_banner_name, ndps_tree, foffset);
                break;
            case 3:     /* Font Types */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_font_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 4:     /* Generic Files/ Archive */
            case 5:     /* Printer Driver Archive */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_archive_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            break;
        case 0x00000007:    /* Get Resource File Date */
            proto_tree_add_item(ndps_tree, hf_ndps_status_flags, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPResAddInputData */
            proto_tree_add_item(ndps_tree, hf_res_type, tvb, foffset, 4, FALSE);
            resource_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (resource_type) 
            {
            case 0:     /* Print Drivers */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 1:     /* Printer Definitions */
                foffset = ndps_string(tvb, hf_ndps_vendor_dir, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 2:     /* Banner Page Files */
                foffset = ndps_string(tvb, hf_ndps_banner_name, ndps_tree, foffset);
                break;
            case 3:     /* Font Types */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_font_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_file_name, ndps_tree, foffset);
                break;
            case 4:     /* Generic Files/ Archive */
            case 5:     /* Printer Driver Archive */
                proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_prn_dir_name, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_archive_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                break;
            default:
                break;
            }
            /* End of NWDPResAddInputData */
            break;
        case 0x0000000a:    /* Set Resource Language Context */
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        default:
            break;
        }
        break;
    case 0x06097b:  /* Delivery */
        switch(ndps_func)
        {
        case 0x00000001:    /* Delivery Bind */
            /* Start of credentials */
            cred_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_item(ndps_tree, hf_ndps_cred_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            switch (cred_type)
            {
            case 0:
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    length = tvb_get_ntohl(tvb, foffset);
                    foffset += 4;
                    proto_tree_add_item(ndps_tree, hf_ndps_password, tvb, foffset, length, FALSE);
                    foffset += length;
                }
                break;
            case 1:
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_certified, tvb, foffset, length, FALSE);
                foffset += length;
                break;
            case 2:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                break;
            case 3:
                name_len = tvb_get_ntohl(tvb, foffset);
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                break;
            case 4:
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                foffset += align_4(tvb, foffset);
                foffset += 2;
                proto_tree_add_item(ndps_tree, hf_ndps_connection, tvb, foffset, 
                2, FALSE);
                foffset += 2;
                foffset = ndps_string(tvb, hf_ndps_user_name, ndps_tree, foffset);
                foffset += 8;   /* Don't know what these 8 bytes signify */
                proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
                4, FALSE);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                break;
            default:
                break;
            }
            /* End of credentials */
            break;
        case 0x00000002:    /* Delivery Unbind */
            /* NoOp */
            break;
        case 0x00000003:    /* Delivery Send */
            proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
                foffset += 4;
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
                proto_tree_add_item(ndps_tree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Containing Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Containing Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentification(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Filter Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentification(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                foffset = attribute_value(tvb, ndps_tree, foffset);
                foffset = ndps_string(tvb, hf_ndps_message, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_time, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of QualifiedName */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Account");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                qualified_name_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                4, qualified_name_type);
                foffset += 4;
                if (qualified_name_type != 0) {
                    if (qualified_name_type == 1) {
                        foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                        foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    }
                }
                /* End of QualifiedName */
            }
            break;
        case 0x00000004:    /* Delivery Send2 */
            proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
                foffset += 4;
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Supplier ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(atree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
                foffset += length;
                proto_tree_add_item(ndps_tree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Containing Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Containing Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentification(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Filter Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Class");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentification(tvb, atree, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object ID");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                /* Start of AttributeSet */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Attribute");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, atree, foffset);  
                }
                /* End of AttributeSet */
                foffset = ndps_string(tvb, hf_ndps_message, ndps_tree, foffset);
                proto_tree_add_item(ndps_tree, hf_time, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of QualifiedName */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Account");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                qualified_name_type = tvb_get_ntohl(tvb, foffset);
                proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
                4, qualified_name_type);
                foffset += 4;
                if (qualified_name_type != 0) {
                    if (qualified_name_type == 1) {
                        foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                    }
                    else
                    {
                        foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                        foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                    }
                }
                /* End of QualifiedName */
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    /*proto_tree_add_uint_format(ndps_tree, hf_ndps_xid, tvb, 0, 
    0, ndps_xid, "This is a Request Packet, XID %08x, Prog %08x, Func %08x", ndps_xid, ndps_prog, ndps_func);*/
    return;
}

static int
ndps_error(tvbuff_t *tvb, packet_info *pinfo, proto_tree *ndps_tree, int foffset)
{
    guint32     number_of_items=0;
    guint32     problem_type=0;
    int         i;
    proto_tree  *atree;
    proto_item  *aitem;

    problem_type = tvb_get_ntohl(tvb, foffset);
    /*if (problem_type == 0)
    {
        return FALSE;
    }*/
    if (check_col(pinfo->cinfo, COL_INFO))
        col_append_str(pinfo->cinfo, COL_INFO, "- Error");
    proto_tree_add_item(ndps_tree, hf_ndps_problem_type, tvb, foffset, 4, FALSE);
    foffset += 4;
    switch(problem_type)
    {
    case 0:                 /* Security Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_security_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        /* Start of NameorID */
        aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Message");
        atree = proto_item_add_subtree(aitem, ett_ndps);
        foffset = name_or_id(tvb, atree, foffset);
        /* End of NameorID */
        break;
    case 1:                 /* Service Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_service_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        foffset = objectidentification(tvb, ndps_tree, foffset);
        foffset = attribute_value(tvb, ndps_tree, foffset);  /* Object Attribute Set */
        proto_tree_add_item(ndps_tree, hf_ndps_lib_error, tvb, foffset, 4, FALSE);
        foffset += 4;
        proto_tree_add_item(ndps_tree, hf_ndps_other_error, tvb, foffset, 4, FALSE);
        foffset += 4;
        proto_tree_add_item(ndps_tree, hf_ndps_other_error_2, tvb, foffset, 4, FALSE);
        foffset += 4;
        break;
    case 2:                 /* Access Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_access_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        foffset = objectidentification(tvb, ndps_tree, foffset);
        break;
    case 3:                 /* Printer Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_printer_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        foffset = objectidentification(tvb, ndps_tree, foffset);
        break;
    case 4:                 /* Selection Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_selection_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        foffset = objectidentification(tvb, ndps_tree, foffset);
        foffset = attribute_value(tvb, ndps_tree, foffset);  /* Object Attribute Set */
        break;
    case 5:                 /* Document Access Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_doc_access_problem_type, tvb, foffset, 4, FALSE);
            foffset = objectidentifier(tvb, ndps_tree, foffset);
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        foffset = objectidentification(tvb, ndps_tree, foffset);
        break;
    case 6:                 /* Attribute Error */
        number_of_items = tvb_get_ntohl(tvb, foffset); 
        aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
        atree = proto_item_add_subtree(aitem, ett_ndps);
        foffset += 4;
        for (i = 1 ; i <= number_of_items; i++ )
        {
            proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
            {
                proto_tree_add_item(ndps_tree, hf_attribute_problem_type, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            else                /* Extended Error */
            {
                /* Start of objectidentifier */
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset = objectidentifier(tvb, atree, foffset);
                /* End of objectidentifier */
            }
            foffset = attribute_value(tvb, ndps_tree, foffset);  /* Object Attribute Set */
        }
        break;
    case 7:                 /* Update Error */
        proto_tree_add_item(ndps_tree, hf_problem_type, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_get_ntohl(tvb, foffset-4)==0) /* Standard Error */
        {
            proto_tree_add_item(ndps_tree, hf_update_problem_type, tvb, foffset, 4, FALSE);
            foffset += 4;
        }
        else                /* Extended Error */
        {
            /* Start of objectidentifier */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Extended Error");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = objectidentifier(tvb, atree, foffset);
            /* End of objectidentifier */
        }
        foffset = objectidentification(tvb, ndps_tree, foffset);
        break;
    default:
        break;
    }
    return foffset;
}

static void
dissect_ndps_reply(tvbuff_t *tvb, packet_info *pinfo, proto_tree *ndps_tree, int foffset)
{
    conversation_t          *conversation = NULL;
    ndps_req_hash_value     *request_value = NULL;
    proto_tree              *atree;
    proto_item              *aitem;
    proto_tree              *btree;
    proto_item              *bitem;
    proto_tree              *ctree;
    proto_item              *citem;
    guint32                 i;
    guint32                 j;
    guint32                 k;
    guint32                 number_of_items=0;
    guint32                 number_of_items2=0;
    guint32                 number_of_items3=0;
    guint32                 length=0;
    guint32                 ndps_func=0;
    guint32                 ndps_prog=0;
    guint32                 error_val=0;
    guint32                 problem_type=0;
    guint32                 field_len=0;
    guint32                 resource_type=0;
    guint32                 qualified_name_type=0;
    guint32                 data_type=0;
    
    if (!pinfo->fd->flags.visited) {
        /* Find the conversation whence the request would have come. */
        conversation = find_conversation(&pinfo->src, &pinfo->dst,
            PT_NCP, (guint32) pinfo->destport, (guint32) pinfo->destport, 0);
        if (conversation != NULL) {
            /* find the record telling us the request made that caused
            this reply */
            request_value = ndps_hash_lookup(conversation, (guint32) pinfo->destport);
            p_add_proto_data(pinfo->fd, proto_ndps, (void*) request_value);
        }
        /* else... we haven't seen an NDPS Request for that conversation. */
    }
    else {
        request_value = p_get_proto_data(pinfo->fd, proto_ndps);
    }
    if (request_value) {
        ndps_prog = request_value->ndps_prog;
        ndps_func = request_value->ndps_func;
        proto_tree_add_uint_format(ndps_tree, hf_ndps_reqframe, tvb, 0, 
           0, request_value->ndps_frame_num,
           "Response to Request in Frame Number: %u",
           request_value->ndps_frame_num);
    }

    if (tvb_length_remaining(tvb, foffset) < 12 && tvb_get_ntohl(tvb, foffset) == 0) /* No error and no return data */
    {
        proto_tree_add_uint(ndps_tree, hf_ndps_error_val, tvb, foffset, 4, error_val);
        if (check_col(pinfo->cinfo, COL_INFO))
                col_append_str(pinfo->cinfo, COL_INFO, "- Ok");
        return;
    }
    if(ndps_func == 1 || ndps_func == 2)
    {
        proto_tree_add_item(ndps_tree, hf_ndps_rpc_acc_stat, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_length_remaining(tvb,foffset) < 4 ) {
            if (check_col(pinfo->cinfo, COL_INFO))
                col_append_str(pinfo->cinfo, COL_INFO, "- Error");
            return;
        }
        proto_tree_add_item(ndps_tree, hf_ndps_rpc_acc_results, tvb, foffset, 4, FALSE);
        foffset += 4;
        if (tvb_length_remaining(tvb,foffset) < 4) {
            if (check_col(pinfo->cinfo, COL_INFO))
                col_append_str(pinfo->cinfo, COL_INFO, "- Error");
            return;
        }
    }
    error_val = tvb_get_ntohl(tvb, foffset);
    proto_tree_add_uint(ndps_tree, hf_ndps_error_val, tvb, foffset, 4, error_val);
    foffset += 4;
    if (check_col(pinfo->cinfo, COL_INFO))
        col_append_str(pinfo->cinfo, COL_INFO, "- Ok");
    switch(ndps_prog)
    {
    case 0x060976:  /* Print */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind PSM */
            proto_tree_add_item(ndps_tree, hf_ndps_oid, tvb, foffset, 4, FALSE);
            foffset += 4;
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
                if(tvb_length_remaining(tvb, foffset) < 4)
                {
                    break;
                }
                proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "PSM Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            break;
        case 0x00000002:    /* Bind PA */
            proto_tree_add_item(ndps_tree, hf_ndps_oid, tvb, foffset, 4, FALSE);
            foffset += 4;
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
                if(tvb_length_remaining(tvb, foffset) < 4)
                {
                    break;
                }
                proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
                foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            break;
        case 0x00000003:    /* Unbind */
            break;
        case 0x00000004:    /* Print */
            foffset = ndps_string(tvb, hf_ndps_pa_name, ndps_tree, foffset);
            proto_tree_add_item(ndps_tree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000005:    /* Modify Job */
        case 0x00000006:    /* Cancel Job */
        case 0x00000008:    /* Promote Job */
        case 0x0000000b:    /* Resume */
        case 0x0000000d:    /* Create */
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Attribute Set");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Object Attribute Set */
            }
            /* End of AttributeSet */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000007:    /* List Object Attributes */
            proto_tree_add_item(ndps_tree, hf_answer_time, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Continuation Option */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Continuation Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items=tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length=tvb_get_ntohl(tvb, foffset);
                if(tvb_length_remaining(tvb, foffset) < length)
                {
                    return;
                }
                proto_tree_add_item(atree, hf_ndps_item_ptr, tvb, foffset, length, FALSE);
                foffset += length;
            }
            /* Limit Encountered Option */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Limit Encountered Option");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_len, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(atree, hf_limit_enc, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Object Results Set */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Results Set");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items=tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = objectidentification(tvb, atree, foffset);
                number_of_items2 = tvb_get_ntohl(tvb, foffset);
                bitem = proto_tree_add_item(atree, hf_ndps_objects, tvb, foffset, 4, FALSE);
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset += 4;
                foffset = objectidentifier(tvb, btree, foffset);
                for (j = 1 ; j <= number_of_items2; j++ )
                {
                    number_of_items3 = tvb_get_ntohl(tvb, foffset);
                    citem = proto_tree_add_item(btree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                    ctree = proto_item_add_subtree(citem, ett_ndps);
                    foffset += 4;
                    for (k = 1 ; k <= number_of_items3; k++ )
                    {
                        foffset = attribute_value(tvb, ctree, foffset);
                    }
                    foffset += align_4(tvb, foffset);
                    proto_tree_add_item(btree, hf_ndps_qualifier, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    foffset = objectidentifier(tvb, btree, foffset);
                }
                foffset += 2;
            }
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000009:    /* Interrupt */
        case 0x0000000a:    /* Pause */
            /* Start of NWDPPrtContainedObjectId */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
            proto_tree_add_item(atree, hf_local_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPPrtContainedObjectId */
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object Attribute Set");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Object Attribute Set */
            }
            /* End of AttributeSet */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x0000000c:    /* Clean */
        case 0x0000000e:    /* Delete */
        case 0x0000000f:    /* Disable PA */
        case 0x00000010:    /* Enable PA */
        case 0x00000012:    /* Set */
        case 0x00000013:    /* Shutdown PA */
        case 0x00000014:    /* Startup PA */
        case 0x00000018:    /* Transfer Data */
        case 0x00000019:    /* Device Control */
        case 0x0000001b:    /* Remove Event Profile */
        case 0x0000001c:    /* Modify Event Profile */
        case 0x0000001e:    /* Shutdown PSM */
        case 0x0000001f:    /* Cancel PSM Shutdown */
        case 0x00000020:    /* Set Printer DS Information */
        case 0x00000021:    /* Clean User Jobs */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000011:    /* Resubmit Jobs */
            number_of_items = tvb_get_ntohl(tvb, foffset); /* Start of ResubmitJob Set */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Resubmit Job");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of NWDPPrtContainedObjectId */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Old Job");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = ndps_string(tvb, hf_ndps_pa_name, btree, foffset);
                proto_tree_add_item(btree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of NWDPPrtContainedObjectId */
                /* Start of NWDPPrtContainedObjectId */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "New Job");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = ndps_string(tvb, hf_ndps_pa_name, btree, foffset);
                proto_tree_add_item(btree, hf_local_id, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of NWDPPrtContainedObjectId */
                /* Start of AttributeSet */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Job Status");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                proto_tree_add_item(btree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = attribute_value(tvb, btree, foffset);  /* Object Attribute Set */
                }
                /* End of AttributeSet */
            } /* End of ResubmitJob Set */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000015:    /* Reorder Job */
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Job Status");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Object Attribute Set */
            }
            /* End of AttributeSet */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000016:    /* Pause PA */
        case 0x00000017:    /* Resume PA */
            /* Start of AttributeSet */
            number_of_items = tvb_get_ntohl(tvb, foffset);
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Printer Status");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_attributes, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset = attribute_value(tvb, atree, foffset);  /* Object Attribute Set */
            }
            /* End of AttributeSet */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x0000001a:    /* Add Event Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x0000001d:    /* List Event Profiles */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
            foffset += length;
            /* Start of Eventhandling */
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_persistence, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Consumer Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
            foffset += length;
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Delivery Address");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset += address_item(tvb, atree, foffset);
            }
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = objectidentifier(tvb, atree, foffset);
                foffset = objectidentification(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;

                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items2 = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items2);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items2; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
            }
            /* End of Eventhandling */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_continuation_option, tvb, foffset, length, FALSE);
            foffset += length;
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        case 0x00000022:    /* Map GUID to NDS Name */
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "NDS Printer Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            if(error_val != 0)
            {
                foffset = ndps_error(tvb, pinfo, ndps_tree, foffset);
            }
            break;
        default:
            break;
        }
        break;
    case 0x060977:  /* Broker */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind */
        case 0x00000002:    /* Unbind */
        case 0x00000004:    /* Enable Service */
        case 0x00000005:    /* Disable Service */
        case 0x00000006:    /* Down Broker */
            /* Start of NWDPBrokerReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPBrokerReturnCode */
            break;
        case 0x00000003:    /* List Services */
            number_of_items = tvb_get_ntohl(tvb, foffset); 
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                bitem = proto_tree_add_item(atree, hf_ndps_service_type, tvb, foffset, 4, FALSE);
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset += 4;
                proto_tree_add_item(btree, hf_ndps_service_enabled, tvb, foffset, 4, FALSE);
                foffset += 4;
            }
            /* Start of NWDPBrokerReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPBrokerReturnCode */
            break;
        case 0x00000007:    /* Get Broker NDS Object Name */
            proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
            4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_broker_name, ndps_tree, foffset);
            foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
            /* Start of NWDPBrokerReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPBrokerReturnCode */
            break;
        case 0x00000008:    /* Get Broker Session Information */
        default:
            break;
        }
        break;
    case 0x060978:  /* Registry */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind */
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_attribute_set, tvb, foffset, length, FALSE);
            }
            /* Start of NWDPRegReturnCode */
            /*proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;*/
            /* End of NWDPRegReturnCode */
            break;
        case 0x00000002:    /* Unbind */
            /* NoOp */
            break;
        case 0x00000003:    /* Register Server */
        case 0x00000004:    /* Deregister Server */
        case 0x00000005:    /* Register Registry */
        case 0x00000006:    /* Deregister Registry */
        case 0x00000007:    /* Registry Update */
            /* Start of NWDPRegReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPRegReturnCode */
            break;
        case 0x00000008:    /* List Local Servers */
        case 0x00000009:    /* List Servers */
            number_of_items = tvb_get_ntohl(tvb, foffset); 
            proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of Server Entry */
                foffset = ndps_string(tvb, hf_ndps_server_name, ndps_tree, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_server_type, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                foffset = print_address(tvb, atree, foffset);
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Server Info");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                number_of_items2 = tvb_get_ntohl(tvb, foffset); 
                proto_tree_add_item(btree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                foffset += 4;
                for (j = 1 ; j <= number_of_items2; j++ )
                {
                    data_type = tvb_get_ntohl(tvb, foffset);
                    proto_tree_add_item(btree, hf_ndps_data_item_type, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    switch (data_type) 
                    {
                    case 0:   /* Int8 */
                        proto_tree_add_item(btree, hf_info_int, tvb, foffset, 1, FALSE);
                        foffset++;
                        break;
                    case 1:   /* Int16 */
                        proto_tree_add_item(btree, hf_info_int16, tvb, foffset, 2, FALSE);
                        foffset += 2;
                        break;
                    case 2:   /* Int32 */
                        proto_tree_add_item(btree, hf_info_int32, tvb, foffset, 4, FALSE);
                        foffset += 4;
                        break;
                    case 3:   /* Boolean */
                        proto_tree_add_item(btree, hf_info_boolean, tvb, foffset, 4, FALSE);
                        foffset += 4;
                        break;
                    case 4:   /* String */
                    case 5:   /* Bytes */
                        foffset = ndps_string(tvb, hf_info_string, btree, foffset);
                        break;
                        /*length = tvb_get_ntohl(tvb, foffset);
                        foffset += 4;
                        proto_tree_add_item(btree, hf_info_bytes, tvb, foffset, length, FALSE);
                        foffset += length;
                        foffset += (length%4);*/
                        break;
                    default:
                        break;
                    }
                }
                /* End of Server Entry */
            }
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_continuation_option, tvb, foffset, length, FALSE);
            foffset += length;
            /* Start of NWDPRegReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPRegReturnCode */
            break;
        case 0x0000000a:    /* List Known Registries */
            number_of_items = tvb_get_ntohl(tvb, foffset); 
            proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_client_server_type, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                foffset = ndps_string(tvb, hf_ndps_registry_name, atree, foffset);
                foffset = print_address(tvb, atree, foffset);
            }
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_continuation_option, tvb, foffset, length, FALSE);
            foffset += length;
            /* Start of NWDPRegReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPRegReturnCode */
            break;
        case 0x0000000b:    /* Get Registry NDS Object Name */
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "NDS Printer Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            /* Start of NWDPRegReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPRegReturnCode */
            break;
        case 0x0000000c:    /* Get Registry Session Information */
            proto_tree_add_item(ndps_tree, hf_ndps_session_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_time, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPRegReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPRegReturnCode */
            break;
        default:
            break;
        }
        break;
    case 0x060979:  /* Notify */
        switch(ndps_func)
        {
        case 0x00000001:    /* Notify Bind */
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_attribute_set, tvb, foffset, length, FALSE);
            }
            /* Start of NWDPNotifyReturnCode */
            /*proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;*/
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000002:    /* Notify Unbind */
            /* NoOp */
            break;
        case 0x00000003:    /* Register Supplier */
            proto_tree_add_item(ndps_tree, hf_ndps_session, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of EventObjectSet */
            length = tvb_get_ntohl(tvb, foffset);   /* Len of record */
            if (length > 0) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of objectidentifier */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Class ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                /* End of objectidentifier */
                /* Start of objectidentification */
                bitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                /* End of objectidentification */
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of ObjectItem */
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
                /* End of ObjectItem */
            }
            /* End of EventObjectSet */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000004:    /* Deregister Supplier */
        case 0x0000000b:    /* Report Notification */
        case 0x0000000d:    /* Remove Delivery Method */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000005:    /* Add Profile */
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of EventObjectSet */
            length = tvb_get_ntohl(tvb, foffset);   /* Len of record */
            if (length > 0) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of objectidentifier */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Class ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                /* End of objectidentifier */
                /* Start of objectidentification */
                bitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                /* End of objectidentification */
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of ObjectItem */
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
                /* End of ObjectItem */
            }
            /* End of EventObjectSet */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000006:    /* Remove Profile */
        case 0x00000007:    /* Modify Profile */
        case 0x00000009:    /* Report Event */
            /* Start of EventObjectSet */
            length = tvb_get_ntohl(tvb, foffset);   /* Len of record */
            if (length > 0) 
            {
                aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
                atree = proto_item_add_subtree(aitem, ett_ndps);
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of objectidentifier */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Class ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentifier(tvb, btree, foffset);
                /* End of objectidentifier */
                /* Start of objectidentification */
                bitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Object ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = objectidentification(tvb, btree, foffset);
                /* End of objectidentification */
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* Start of ObjectItem */
                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
                /* End of ObjectItem */
            }
            /* End of EventObjectSet */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000008:    /* List Profiles */
            /* Start of ProfileResultSet */
            proto_tree_add_item(ndps_tree, hf_ndps_len, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of Eventhandling */
            proto_tree_add_item(ndps_tree, hf_ndps_profile_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_persistence, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of QualifiedName */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Consumer Name");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(atree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, atree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_pa_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, atree, foffset);
                }
            }
            /* End of QualifiedName */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_attribute_value, tvb, foffset, length, FALSE);
            foffset += length;
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Delivery Address");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset += address_item(tvb, atree, foffset);
            }
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Event Object");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                proto_tree_add_item(atree, hf_ndps_event_type, tvb, foffset, 4, FALSE);
                foffset += 4;
                foffset = objectidentifier(tvb, atree, foffset);
                foffset = objectidentification(tvb, atree, foffset);
                proto_tree_add_item(atree, hf_ndps_object_op, tvb, foffset, 4, FALSE);
                foffset += 4;

                proto_tree_add_uint(atree, hf_ndps_event_object_identifier, tvb, foffset, 4, FALSE);
                foffset += 4;
                if(tvb_get_ntohl(tvb, foffset-4)==1)
                {
                    foffset = objectidentifier(tvb, atree, foffset);
                }
                else
                {
                    if(tvb_get_ntohl(tvb, foffset-4)==0)
                    {
                        number_of_items2 = tvb_get_ntohl(tvb, foffset);
                        proto_tree_add_uint(atree, hf_ndps_item_count, tvb, foffset, 4, number_of_items2);
                        foffset += 4;
                        for (j = 1 ; j <= number_of_items2; j++ )
                        {
                            foffset = objectidentifier(tvb, atree, foffset);
                        }
                    }
                }
            }
            /* End of Eventhandling */
            /* End of ProfileResultSet */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_continuation_option, tvb, foffset, length, FALSE);
            foffset += length;
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x0000000a:    /* List Supported Languages */
            /* Start of IntegerSeq */
            length = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, length, FALSE);
            foffset += length;
            /* End of IntegerSeq */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x0000000c:    /* Add Delivery Method */
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x0000000e:    /* List Delivery Methods */
            /* Start of DeliveryMethodSet */
            number_of_items = tvb_get_ntohl(tvb, foffset); 
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                /* Start of DeliveryMethod */
                /* Start of NameorID */
                bitem = proto_tree_add_text(atree, tvb, foffset, 0, "Method ID");
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset = name_or_id(tvb, btree, foffset);
                foffset += align_4(tvb, foffset);
                /* End of NameorID */
                foffset = ndps_string(tvb, hf_ndps_method_name, btree, foffset);
                foffset = ndps_string(tvb, hf_ndps_method_ver, btree, foffset);
                foffset = ndps_string(tvb, hf_ndps_file_name, btree, foffset);
                proto_tree_add_item(btree, hf_ndps_admin_submit, tvb, foffset, 4, FALSE);
                foffset += 4;
                /* End of DeliveryMethod */
            }
            /* End of DeliveryMethodSet */
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x0000000f:    /* Get Delivery Method Information */
            /* Start of DeliveryMethod */
            /* Start of NameorID */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Method ID");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset = name_or_id(tvb, atree, foffset);
            /* End of NameorID */
            foffset = ndps_string(tvb, hf_ndps_method_name, atree, foffset);
            foffset = ndps_string(tvb, hf_ndps_method_ver, atree, foffset);
            foffset = ndps_string(tvb, hf_ndps_file_name, atree, foffset);
            proto_tree_add_item(atree, hf_ndps_admin_submit, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of DeliveryMethod */
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Delivery Address");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            number_of_items = tvb_get_ntohl(tvb, foffset);
            for (i = 1 ; i <= number_of_items; i++ )
            {
                foffset += address_item(tvb, atree, foffset);
            }
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000010:    /* Get Notify NDS Object Name */
            proto_tree_add_item(ndps_tree, hf_ndps_items, tvb, foffset,
            4, FALSE);
            foffset += 4;
            foffset = ndps_string(tvb, hf_ndps_broker_name, ndps_tree, foffset);
            foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        case 0x00000011:    /* Get Notify Session Information */
            proto_tree_add_item(ndps_tree, hf_ndps_get_session_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_time, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPNotifyReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPNotifyReturnCode */
            break;
        default:
            break;
        }
        break;
    case 0x06097a:  /* Resman */
        switch(ndps_func)
        {
        case 0x00000001:    /* Bind */
            aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            atree = proto_item_add_subtree(aitem, ett_ndps);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length = tvb_get_ntohl(tvb, foffset);
                foffset += 4;
                proto_tree_add_item(ndps_tree, hf_ndps_attribute_set, tvb, foffset, length, FALSE);
            }
            /* Start of NWDPResManReturnCode */
            /*proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;*/
            /* End of NWDPResManReturnCode */
            break;
        case 0x00000002:    /* Unbind */
            /* NoOp */
            break;
        case 0x00000003:    /* Add Resource File */
        case 0x00000004:    /* Delete Resource File */
            /* Start of NWDPResManReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPResManReturnCode */
            break;
        case 0x00000005:    /* List Resources */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) != 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_status_flags, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_resource_list_type, tvb, foffset, 4, FALSE);
            resource_type = tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            switch (resource_type) 
            {
            case 0:     /* Print Drivers */
            case 1:     /* Printer Definitions */
            case 2:     /* Printer Definitions Short */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    if (tvb_get_ntohl(tvb, foffset)==0) {  /* Offset for old type support */
                        foffset += 2;
                    }
                    foffset += 4; /* Item always == 1 */
                    foffset = ndps_string(tvb, hf_ndps_printer_manuf, atree, foffset);
                    if (tvb_get_ntohl(tvb, foffset)==0) {
                        foffset += 2;
                    }
                    foffset += 4;
                    foffset = ndps_string(tvb, hf_ndps_printer_type, atree, foffset);
                    if (tvb_get_ntohl(tvb, foffset)==0) {
                        foffset += 2;
                    }
                    foffset += 4;
                    foffset = ndps_string(tvb, hf_ndps_inf_file_name, atree, foffset);
                }
                break;
            case 3:     /* Banner Page Files */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_ndps_banner_name, atree, foffset);
                }
                break;
            case 4:     /* Font Types */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_font_type_name, atree, foffset);
                }
                break;
            case 7:     /* Font Files */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_font_file_name, atree, foffset);
                }
                break;
            case 5:     /* Printer Driver Files */
            case 12:    /* Printer Driver Files 2 */
            case 9:     /* Generic Files */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_ndps_prn_file_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_prn_dir_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_inf_file_name, atree, foffset);
                }
                break;
            case 6:     /* Printer Definition File */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_ndps_prn_file_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_prn_dir_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_inf_file_name, atree, foffset);
                }
                number_of_items = tvb_get_ntohl(tvb, foffset);
                bitem = proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                btree = proto_item_add_subtree(bitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_ndps_def_file_name, btree, foffset);
                    number_of_items2 = tvb_get_ntohl(tvb, foffset);
                    citem = proto_tree_add_item(btree, hf_ndps_win31_items, tvb, foffset, 4, FALSE);
                    ctree = proto_item_add_subtree(citem, ett_ndps);
                    foffset += 4;
                    for (i = 1 ; i <= number_of_items2; i++ )
                    {
                        foffset = ndps_string(tvb, hf_ndps_windows_key, ctree, foffset);
                    }
                    number_of_items2 = tvb_get_ntohl(tvb, foffset);
                    citem = proto_tree_add_item(btree, hf_ndps_win95_items, tvb, foffset, 4, FALSE);
                    ctree = proto_item_add_subtree(citem, ett_ndps);
                    foffset += 4;
                    for (i = 1 ; i <= number_of_items2; i++ )
                    {
                        foffset = ndps_string(tvb, hf_ndps_windows_key, ctree, foffset);
                    }
                }
                break;
            case 10:    /* Printer Definition File 2 */
                foffset = ndps_string(tvb, hf_ndps_def_file_name, ndps_tree, foffset);
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    proto_tree_add_item(ndps_tree, hf_os_type, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    number_of_items2 = tvb_get_ntohl(tvb, foffset);
                    bitem = proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                    btree = proto_item_add_subtree(bitem, ett_ndps);
                    foffset += 4;
                    for (i = 1 ; i <= number_of_items2; i++ )
                    {
                        foffset = ndps_string(tvb, hf_ndps_windows_key, btree, foffset);
                    }
                }
                break;
            case 8:     /* Generic Type */
            case 11:    /* Printer Driver Types 2 */
            case 13:    /* Printer Driver Types Archive */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    foffset = ndps_string(tvb, hf_ndps_printer_manuf, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_printer_type, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_prn_file_name, atree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_prn_dir_name, atree, foffset);
                    proto_tree_add_item(atree, hf_archive_type, tvb, foffset, 4, FALSE);
                    foffset += 4;
                    proto_tree_add_item(atree, hf_archive_file_size, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                break;
            case 14:    /* Languages Available */
                number_of_items = tvb_get_ntohl(tvb, foffset);
                aitem = proto_tree_add_item(ndps_tree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
                atree = proto_item_add_subtree(aitem, ett_ndps);
                foffset += 4;
                for (i = 1 ; i <= number_of_items; i++ )
                {
                    proto_tree_add_item(atree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
                    foffset += 4;
                }
                break;
            default:
                break;
            }
            break;
        case 0x00000006:    /* Get Resource File */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) != 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_get_status_flag, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_file_timestamp, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_ndps_data, tvb, foffset, -1, FALSE);
            break;
        case 0x00000007:    /* Get Resource File Date */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) != 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_file_timestamp, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x00000008:    /* Get Resource Manager NDS Object Name */
            qualified_name_type = tvb_get_ntohl(tvb, foffset);
            proto_tree_add_uint(ndps_tree, hf_ndps_qualified_name, tvb, foffset, 
            4, qualified_name_type);
            foffset += 4;
            if (qualified_name_type != 0) {
                if (qualified_name_type == 1) {
                    foffset = ndps_string(tvb, hf_printer_name, ndps_tree, foffset);
                }
                else
                {
                    foffset = ndps_string(tvb, hf_ndps_broker_name, ndps_tree, foffset);
                    foffset = ndps_string(tvb, hf_ndps_tree, ndps_tree, foffset);
                }
            }
            proto_tree_add_uint(ndps_tree, hf_ndps_error_val, tvb, foffset, 4, error_val);
            foffset += 4;
            break;
        case 0x00000009:    /* Get Resource Manager Session Information */
            proto_tree_add_item(ndps_tree, hf_ndps_get_resman_session_type, tvb, foffset, 4, FALSE);
            foffset += 4;
            proto_tree_add_item(ndps_tree, hf_time, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* Start of NWDPResManReturnCode */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) == 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_ext_error, tvb, foffset, 4, FALSE);
            foffset += 4;
            /* End of NWDPResManReturnCode */
            break;
        case 0x0000000a:    /* Set Resource Language Context */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) != 0) 
            {
                break;
            }
            proto_tree_add_item(ndps_tree, hf_ndps_language_id, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        default:
            break;
        }
        break;
    case 0x06097b:  /* Delivery */
        switch(ndps_func)
        {
        case 0x00000001:    /* Delivery Bind */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            break;
        case 0x00000002:    /* Delivery Unbind */
            /* NoOp */
            break;
        case 0x00000003:    /* Delivery Send */
        case 0x00000004:    /* Delivery Send2 */
            proto_tree_add_item(ndps_tree, hf_ndps_return_code, tvb, foffset, 4, FALSE);
            foffset += 4;
            if (tvb_get_ntohl(tvb, foffset-4) != 0) 
            {
                break;
            }
            aitem = proto_tree_add_text(ndps_tree, tvb, foffset, 0, "Failed Items");
            atree = proto_item_add_subtree(aitem, ett_ndps);
            proto_tree_add_item(atree, hf_ndps_item_count, tvb, foffset, 4, FALSE);
            number_of_items=tvb_get_ntohl(tvb, foffset);
            foffset += 4;
            for (i = 1 ; i <= number_of_items; i++ )
            {
                length=tvb_get_ntohl(tvb, foffset);
                if(tvb_length_remaining(tvb, foffset) < length)
                {
                    return;
                }
                proto_tree_add_item(atree, hf_ndps_item_ptr, tvb, foffset, length, FALSE);
                foffset += length;
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return;
}

void
proto_register_ndps(void)
{
	static hf_register_info hf_ndps[] = {
		{ &hf_ndps_record_mark,
		{ "Record Mark",		"ndps.record_mark", FT_UINT16, BASE_HEX, NULL, 0x0,
			"Record Mark", HFILL }},

        { &hf_ndps_packet_type,
        { "Packet Type",    "ndps.packet_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_packet_types),   0x0,
          "Packet Type", HFILL }},

        { &hf_ndps_length,
        { "Record Length",    "ndps.record_length",
           FT_UINT16,    BASE_DEC,   NULL,   0x0,
           "Record Length", HFILL }},
        
        { &hf_ndps_xid,
        { "Exchange ID",    "ndps.xid",
           FT_UINT32,    BASE_HEX,   NULL,   0x0,
           "Exchange ID", HFILL }},

        { &hf_ndps_rpc_version,
        { "RPC Version",    "ndps.rpc_version",
           FT_UINT32,    BASE_HEX,   NULL,   0x0,
           "RPC Version", HFILL }},

        { &hf_spx_ndps_program,
        { "NDPS Program Number",    "spx.ndps_program",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_program_vals),   0x0,
          "NDPS Program Number", HFILL }},
	
        { &hf_spx_ndps_version,
        { "Program Version",    "spx.ndps_version",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Program Version", HFILL }}, 
    
        { &hf_ndps_error,
        { "NDPS Error",    "spx.ndps_error",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "NDPS Error", HFILL }}, 
        
        { &hf_spx_ndps_func_print,
        { "Print Program",    "spx.ndps_func_print",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_print_func_vals),   0x0,
          "Print Program", HFILL }},
        
        { &hf_spx_ndps_func_notify,
        { "Notify Program",    "spx.ndps_func_notify",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_notify_func_vals),   0x0,
          "Notify Program", HFILL }},
        
        { &hf_spx_ndps_func_delivery,
        { "Delivery Program",    "spx.ndps_func_delivery",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_deliver_func_vals),   0x0,
          "Delivery Program", HFILL }},
        
        { &hf_spx_ndps_func_registry,
        { "Registry Program",    "spx.ndps_func_registry",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_registry_func_vals),   0x0,
          "Registry Program", HFILL }},
        
        { &hf_spx_ndps_func_resman,
        { "ResMan Program",    "spx.ndps_func_resman",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_resman_func_vals),   0x0,
          "ResMan Program", HFILL }},
        
        { &hf_spx_ndps_func_broker,
        { "Broker Program",    "spx.ndps_func_broker",
          FT_UINT32,    BASE_HEX,   VALS(spx_ndps_broker_func_vals),   0x0,
          "Broker Program", HFILL }},
        
        { &hf_ndps_items,
        { "Number of Items",    "ndps.items",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Number of Items", HFILL }},

        { &hf_ndps_objects,
        { "Number of Objects",    "ndps.objects",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Number of Obejcts", HFILL }},

        { &hf_ndps_attributes,
        { "Number of Attributes",    "ndps.attributes",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Number of Attributes", HFILL }},

        { &hf_ndps_sbuffer,
        { "Server",    "ndps.sbuffer",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Server", HFILL }},
        
        { &hf_ndps_rbuffer,
        { "Connection",    "ndps.rbuffer",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Connection", HFILL }},

        { &hf_ndps_user_name,
        { "Trustee Name",    "ndps.user_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Trustee Name", HFILL }},
        
        { &hf_ndps_broker_name,
        { "Broker Name",    "ndps.broker_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Broker Name", HFILL }},

        { &hf_ndps_pa_name,
        { "Printer Name",    "ndps.pa_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer Name", HFILL }},
        
        { &hf_ndps_tree,
        { "Tree",    "ndps.tree",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Tree", HFILL }},

        { &hf_ndps_reqframe,
        { "Request Frame",    "ndps.reqframe",
          FT_FRAMENUM,  BASE_NONE,   NULL,   0x0,
          "Request Frame", HFILL }},

        { &hf_ndps_error_val,
        { "Return Status",    "ndps.error_val",
          FT_UINT32,    BASE_HEX,   VALS(ndps_error_types),   0x0,
          "Return Status", HFILL }},

        { &hf_ndps_object,
        { "Object ID",    "ndps.object",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Object ID", HFILL }},

        { &hf_ndps_cred_type,
        { "Credential Type",    "ndps.cred_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_credential_enum),   0x0,
          "Credential Type", HFILL }},

        { &hf_ndps_server_name,
        { "Server Name",    "ndps.server_name",
          FT_STRING,    BASE_DEC,   NULL,   0x0,
          "Server Name", HFILL }},

        { &hf_ndps_connection,
        { "Connection",    "ndps.connection",
          FT_UINT16,    BASE_DEC,   NULL,   0x0,
          "Connection", HFILL }},

        { &hf_ndps_ext_error,
        { "Extended Error Code",    "ndps.ext_error",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Extended Error Code", HFILL }},

        { &hf_ndps_auth_null,
        { "Auth Null",    "ndps.auth_null",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Auth Null", HFILL }},

        { &hf_ndps_rpc_accept,
        { "RPC Accept or Deny",    "ndps.rpc_acc",
          FT_UINT32,    BASE_HEX,   VALS(true_false),   0x0,
          "RPC Accept or Deny", HFILL }},

        { &hf_ndps_rpc_acc_stat,
        { "RPC Accept Status",    "ndps.rpc_acc_stat",
          FT_UINT32,    BASE_HEX,   VALS(accept_stat),   0x0,
          "RPC Accept Status", HFILL }},
        
        { &hf_ndps_rpc_rej_stat,
        { "RPC Reject Status",    "ndps.rpc_rej_stat",
          FT_UINT32,    BASE_HEX,   VALS(reject_stat),   0x0,
          "RPC Reject Status", HFILL }},
        
        { &hf_ndps_rpc_acc_results,
        { "RPC Accept Results",    "ndps.rpc_acc_res",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "RPC Accept Results", HFILL }},

        { &hf_ndps_problem_type,
        { "Problem Type",    "ndps.rpc_prob_type",
          FT_UINT32,    BASE_HEX,   VALS(error_type_enum),   0x0,
          "Problem Type", HFILL }},
    
        { &hf_security_problem_type,
        { "Security Problem",    "ndps.rpc_sec_prob",
          FT_UINT32,    BASE_HEX,   VALS(security_problem_enum),   0x0,
          "Security Problem", HFILL }},

        { &hf_service_problem_type,
        { "Service Problem",    "ndps.rpc_serv_prob",
          FT_UINT32,    BASE_HEX,   VALS(service_problem_enum),   0x0,
          "Service Problem", HFILL }},
        
        { &hf_access_problem_type,
        { "Access Problem",    "ndps.rpc_acc_prob",
          FT_UINT32,    BASE_HEX,   VALS(access_problem_enum),   0x0,
          "Access Problem", HFILL }},
        
        { &hf_printer_problem_type,
        { "Printer Problem",    "ndps.rpc_print_prob",
          FT_UINT32,    BASE_HEX,   VALS(printer_problem_enum),   0x0,
          "Printer Problem", HFILL }},
        
        { &hf_selection_problem_type,
        { "Selection Problem",    "ndps.rpc_sel_prob",
          FT_UINT32,    BASE_HEX,   VALS(selection_problem_enum),   0x0,
          "Selection Problem", HFILL }},
        
        { &hf_doc_access_problem_type,
        { "Document Access Problem",    "ndps.rpc_doc_acc_prob",
          FT_UINT32,    BASE_HEX,   VALS(doc_access_problem_enum),   0x0,
          "Document Access Problem", HFILL }},
        
        { &hf_attribute_problem_type,
        { "Attribute Problem",    "ndps.rpc_attr_prob",
          FT_UINT32,    BASE_HEX,   VALS(attribute_problem_enum),   0x0,
          "Attribute Problem", HFILL }},

        { &hf_update_problem_type,
        { "Update Problem",    "ndps.rpc_update_prob",
          FT_UINT32,    BASE_HEX,   VALS(update_problem_enum),   0x0,
          "Update Problem", HFILL }},
        
        { &hf_obj_id_type,
        { "Object ID Type",    "ndps.rpc_obj_id_type",
          FT_UINT32,    BASE_HEX,   VALS(obj_identification_enum),   0x0,
          "Object ID Type", HFILL }},

        { &hf_oid_struct_size,
        { "OID Struct Size",    "ndps.rpc_oid_struct_size",
          FT_UINT16,    BASE_HEX,   NULL,   0x0,
          "OID Struct Size", HFILL }},
        
        { &hf_object_name,
        { "Object Name",    "ndps.ndps_object_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Object Name", HFILL }},

        { &hf_ndps_document_number,
        { "Document Number",    "ndps.ndps_doc_num",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Document Number", HFILL }},

        { &hf_ndps_nameorid,
        { "Name or ID Type",    "ndps.ndps_nameorid",
          FT_UINT32,    BASE_HEX,   VALS(nameorid_enum),   0x0,
          "Name or ID Type", HFILL }},

        { &hf_local_object_name,
        { "Local Object Name",    "ndps.ndps_loc_object_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Local Object Name", HFILL }},

        { &hf_printer_name,
        { "Printer Name",    "ndps.ndps_printer_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer Name", HFILL }},

        { &hf_ndps_qualified_name,
        { "Qualified Name Type",    "ndps.ndps_qual_name_type",
          FT_UINT32,    BASE_HEX,   VALS(qualified_name_enum),   0x0,
          "Qualified Name Type", HFILL }},

        { &hf_ndps_qualified_name2,
        { "Qualified Name Type",    "ndps.ndps_qual_name_type2",
          FT_UINT32,    BASE_HEX,   VALS(qualified_name_enum2),   0x0,
          "Qualified Name Type", HFILL }},
        

        { &hf_ndps_item_count,
        { "Number of Items",    "ndps.ndps_item_count",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Number of Items", HFILL }},

        { &hf_ndps_qualifier,
        { "Qualifier",    "ndps.ndps_qual",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Qualifier", HFILL }},

        { &hf_ndps_lib_error,
        { "Lib Error",    "ndps.ndps_lib_error",
          FT_UINT32,    BASE_HEX,   VALS(ndps_error_types),   0x0,
          "Lib Error", HFILL }},

        { &hf_ndps_other_error,
        { "Other Error",    "ndps.ndps_other_error",
          FT_UINT32,    BASE_HEX,   VALS(ndps_error_types),   0x0,
          "Other Error", HFILL }},

        { &hf_ndps_other_error_2,
        { "Other Error 2",    "ndps.ndps_other_error_2",
          FT_UINT32,    BASE_HEX,   VALS(ndps_error_types),   0x0,
          "Other Error 2", HFILL }},

        { &hf_ndps_session,
        { "Session Handle",    "ndps.ndps_session",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Session Handle", HFILL }},

        { &hf_ndps_abort_flag,
        { "Abort?",    "ndps.ndps_abort",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Abort?", HFILL }},

        { &hf_obj_attribute_type,
        { "Attribute Type",    "ndps.ndps_attrib_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_attribute_enum),   0x0,
          "Attribute Type", HFILL }},

        { &hf_ndps_attribute_value,
        { "Value",    "ndps.attribue_value",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Value", HFILL }},

        { &hf_ndps_lower_range,
        { "Lower Range",    "ndps.lower_range",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Lower Range", HFILL }},

        { &hf_ndps_upper_range,
        { "Upper Range",    "ndps.upper_range",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Upper Range", HFILL }},

        { &hf_ndps_n64,
        { "Value",    "ndps.n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Value", HFILL }},

        { &hf_ndps_lower_range_n64,
        { "Lower Range",    "ndps.lower_range_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Lower Range", HFILL }},

        { &hf_ndps_upper_range_n64,
        { "Upper Range",    "ndps.upper_range_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Upper Range", HFILL }},

        { &hf_ndps_attrib_boolean,
        { "Value?",    "ndps.ndps_attrib_boolean",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Value?", HFILL }},

        { &hf_ndps_realization,
        { "Realization Type",    "ndps.ndps_realization",
          FT_UINT32,    BASE_HEX,   VALS(ndps_realization_enum),   0x0,
          "Realization Type", HFILL }},

        { &hf_ndps_xdimension_n64,
        { "X Dimension",    "ndps.xdimension_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "X Dimension", HFILL }},

        { &hf_ndps_ydimension_n64,
        { "Y Dimension",    "ndps.xdimension_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Y Dimension", HFILL }},

        { &hf_ndps_dim_value,
        { "Dimension Value Type",    "ndps.ndps_dim_value",
          FT_UINT32,    BASE_HEX,   VALS(ndps_dim_value_enum),   0x0,
          "Dimension Value Type", HFILL }},

        { &hf_ndps_dim_flag,
        { "Dimension Flag",    "ndps.ndps_dim_falg",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Dimension Flag", HFILL }},

        { &hf_ndps_xydim_value,
        { "XY Dimension Value Type",    "ndps.ndps_xydim_value",
          FT_UINT32,    BASE_HEX,   VALS(ndps_xydim_value_enum),   0x0,
          "XY Dimension Value Type", HFILL }},

        { &hf_ndps_location_value,
        { "Location Value Type",    "ndps.ndps_location_value",
          FT_UINT32,    BASE_HEX,   VALS(ndps_location_value_enum),   0x0,
          "Location Value Type", HFILL }},

        { &hf_ndps_xmin_n64,
        { "Minimum X Dimension",    "ndps.xmin_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Minimum X Dimension", HFILL }},

        { &hf_ndps_xmax_n64,
        { "Maximum X Dimension",    "ndps.xmax_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Maximum X Dimension", HFILL }},

        { &hf_ndps_ymin_n64,
        { "Minimum Y Dimension",    "ndps.ymin_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Minimum Y Dimension", HFILL }},

        { &hf_ndps_ymax_n64,
        { "Maximum Y Dimension",    "ndps.ymax_n64",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Maximum Y Dimension", HFILL }},

        { &hf_ndps_edge_value,
        { "Edge Value",    "ndps.ndps_edge_value",
          FT_UINT32,    BASE_HEX,   VALS(ndps_edge_value_enum),   0x0,
          "Edge Value", HFILL }},

        { &hf_ndps_cardinal_or_oid,
        { "Cardinal or OID",    "ndps.ndps_car_or_oid",
          FT_UINT32,    BASE_HEX,   VALS(ndps_card_or_oid_enum),   0x0,
          "Cardinal or OID", HFILL }},

        { &hf_ndps_cardinal_name_or_oid,
        { "Cardinal Name or OID",    "ndps.ndps_car_name_or_oid",
          FT_UINT32,    BASE_HEX,   VALS(ndps_card_name_or_oid_enum),   0x0,
          "Cardinal Name or OID", HFILL }},

        { &hf_ndps_integer_or_oid,
        { "Integer or OID",    "ndps.ndps_integer_or_oid",
          FT_UINT32,    BASE_HEX,   VALS(ndps_integer_or_oid_enum),   0x0,
          "Integer or OID", HFILL }},

        { &hf_ndps_profile_id,
        { "Profile ID",    "ndps.ndps_profile_id",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Profile ID", HFILL }},

        { &hf_ndps_persistence,
        { "Persistence",    "ndps.ndps_persistence",
          FT_UINT32,    BASE_HEX,   VALS(ndps_persistence_enum),   0x0,
          "Persistence", HFILL }},

        { &hf_ndps_language_id,
        { "Language ID",    "ndps.ndps_lang_id",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Language ID", HFILL }},

        { &hf_address_type,
        { "Address Type",    "ndps.ndps_address_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_address_type_enum),   0x0,
          "Address Type", HFILL }},

        { &hf_ndps_address,
        { "Address",    "ndps.ndps_address",
          FT_UINT32,    BASE_HEX,   VALS(ndps_address_enum),   0x0,
          "Address", HFILL }},

        { &hf_ndps_add_bytes,
        { "Address Bytes",    "ndps.add_bytes",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Address Bytes", HFILL }},

        { &hf_ndps_event_type,
        { "Event Type",    "ndps.ndps_event_type",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Event Type", HFILL }},

        { &hf_ndps_event_object_identifier,
        { "Event Object Type",    "ndps.ndps_event_object_identifier",
          FT_UINT32,    BASE_HEX,   VALS(ndps_event_object_enum),   0x0,
          "Event Object Type", HFILL }},

        { &hf_ndps_octet_string,
        { "Octet String",    "ndps.octet_string",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Octet String", HFILL }},

        { &hf_ndps_scope,
        { "Scope",    "ndps.scope",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Scope", HFILL }},

        { &hf_address_len,
        { "Address Length",    "ndps.addr_len",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Address Length", HFILL }},

        { &hf_ndps_net,
        { "IPX Network",    "ndps.net",
          FT_IPXNET,    BASE_NONE,   NULL,   0x0,
          "Scope", HFILL }},

        { &hf_ndps_node,
        { "Node",    "ndps.node",
          FT_ETHER,    BASE_NONE,   NULL,   0x0,
          "Node", HFILL }},

        { &hf_ndps_socket,
        { "IPX Socket",    "ndps.socket",
          FT_UINT16,    BASE_HEX,   NULL,   0x0,
          "IPX Socket", HFILL }},

        { &hf_ndps_port,
        { "IP Port",    "ndps.port",
          FT_UINT16,    BASE_DEC,   NULL,   0x0,
          "IP Port", HFILL }},

        { &hf_ndps_ip,
        { "IP Address",    "ndps.ip",
          FT_IPv4,    BASE_DEC,   NULL,   0x0,
          "IP Address", HFILL }},
        
        { &hf_ndps_server_type,
        { "NDPS Server Type",    "ndps.ndps_server_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_server_type_enum),   0x0,
          "NDPS Server Type", HFILL }},

        { &hf_ndps_service_type,
        { "NDPS Service Type",    "ndps.ndps_service_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_service_type_enum),   0x0,
          "NDPS Service Type", HFILL }},
    
        { &hf_ndps_service_enabled,
        { "Service Enabled?",    "ndps.ndps_service_enabled",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Service Enabled?", HFILL }},

        { &hf_ndps_method_name,
        { "Method Name",    "ndps.method_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Method Name", HFILL }},

        { &hf_ndps_method_ver,
        { "Method Version",    "ndps.method_ver",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Method Version", HFILL }},

        { &hf_ndps_file_name,
        { "File Name",    "ndps.file_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "File Name", HFILL }},
        
        { &hf_ndps_admin_submit,
        { "Admin Submit Flag?",    "ndps.admin_submit_flag",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Admin Submit Flag?", HFILL }},

        { &hf_ndps_oid,
        { "Object ID",    "ndps.oid",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Object ID", HFILL }},

        { &hf_ndps_object_op,
        { "Operation",    "ndps.ndps_object_op",
          FT_UINT32,    BASE_HEX,   VALS(ndps_object_op_enum),   0x0,
          "Operation", HFILL }},

        { &hf_answer_time,
        { "Answer Time",    "ndps.answer_time",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Answer Time", HFILL }},
        
        { &hf_oid_asn1_type,
        { "ASN.1 Type",    "ndps.asn1_type",
          FT_UINT16,    BASE_DEC,   NULL,   0x0,
          "ASN.1 Type", HFILL }},

        { &hf_ndps_item_ptr,
        { "Item Pointer",    "ndps.item_ptr",
          FT_BYTES,    BASE_DEC,   NULL,   0x0,
          "Item Pointer", HFILL }},

        { &hf_ndps_len,
        { "Length",    "ndps.ndps_len",
          FT_UINT16,    BASE_DEC,   NULL,   0x0,
          "Length", HFILL }},
     
        { &hf_limit_enc,
        { "Limit Encountered",    "ndps.ndps_limit_enc",
          FT_UINT32,    BASE_HEX,   VALS(ndps_limit_enc_enum),   0x0,
          "Limit Encountered", HFILL }},
        
        { &hf_ndps_delivery_add_type,
        { "Delivery Address Type",    "ndps.ndps_delivery_add_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_delivery_add_enum),   0x0,
          "Delivery Address Type", HFILL }},

        { &hf_ndps_criterion_type,
        { "Criterion Type",    "ndps.ndps_criterion_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_attribute_enum),   0x0,
          "Criterion Type", HFILL }},

        { &hf_ndps_ignored_type,
        { "Ignored Type",    "ndps.ndps_ignored_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_attribute_enum),   0x0,
          "Ignored Type", HFILL }},

        { &hf_ndps_resource_type,
        { "Resource Type",    "ndps.ndps_resource_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_resource_enum),   0x0,
          "Resource Type", HFILL }},
      
        { &hf_ndps_identifier_type,
        { "Identifier Type",    "ndps.ndps_identifier_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_identifier_enum),   0x0,
          "Identifier Type", HFILL }},

        { &hf_ndps_page_flag,
        { "Page Flag",    "ndps.ndps_page_flag",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Page Flag", HFILL }},
        
        { &hf_ndps_media_type,
        { "Media Type",    "ndps.ndps_media_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_media_enum),   0x0,
          "Media Type", HFILL }},

        { &hf_ndps_page_size,
        { "Page Size",    "ndps.ndps_page_size",
          FT_UINT32,    BASE_HEX,   VALS(ndps_page_size_enum),   0x0,
          "Page Size", HFILL }},
     
        { &hf_ndps_direction,
        { "Direction",    "ndps.ndps_direction",
          FT_UINT32,    BASE_HEX,   VALS(ndps_pres_direction_enum),   0x0,
          "Direction", HFILL }},
       
        { &hf_ndps_page_order,
        { "Page Order",    "ndps.ndps_page_order",
          FT_UINT32,    BASE_HEX,   VALS(ndps_page_order_enum),   0x0,
          "Page Order", HFILL }},

        { &hf_ndps_medium_size,
        { "Medium Size",    "ndps.ndps_medium_size",
          FT_UINT32,    BASE_HEX,   VALS(ndps_medium_size_enum),   0x0,
          "Medium Size", HFILL }},

        { &hf_ndps_long_edge_feeds,
        { "Long Edge Feeds?",    "ndps.ndps_long_edge_feeds",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Long Edge Feeds?", HFILL }},

        { &hf_ndps_inc_across_feed,
        { "Increment Across Feed",    "ndps.inc_across_feed",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Increment Across Feed", HFILL }},

        { &hf_ndps_size_inc_in_feed,
        { "Size Increment in Feed",    "ndps.size_inc_in_feed",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Size Increment in Feed", HFILL }},

        { &hf_ndps_page_orientation,
        { "Page Orientation",    "ndps.ndps_page_orientation",
          FT_UINT32,    BASE_HEX,   VALS(ndps_page_orientation_enum),   0x0,
          "Page Orientation", HFILL }},

        { &hf_ndps_numbers_up,
        { "Numbers Up",    "ndps.ndps_numbers_up",
          FT_UINT32,    BASE_HEX,   VALS(ndps_numbers_up_enum),   0x0,
          "Numbers Up", HFILL }},

        { &hf_ndps_xdimension,
        { "X Dimension",    "ndps.ndps_xdimension",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "X Dimension", HFILL }},
        
        { &hf_ndps_ydimension,
        { "Y Dimension",    "ndps.ndps_ydimension",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Y Dimension", HFILL }},
        
        { &hf_ndps_state_severity,
        { "State Severity",    "ndps.ndps_state_severity",
          FT_UINT32,    BASE_HEX,   VALS(ndps_state_severity_enum),   0x0,
          "State Severity", HFILL }},

        { &hf_ndps_training,
        { "Training",    "ndps.ndps_training",
          FT_UINT32,    BASE_HEX,   VALS(ndps_training_enum),   0x0,
          "Training", HFILL }},

        { &hf_ndps_colorant_set,
        { "Colorant Set",    "ndps.ndps_colorant_set",
          FT_UINT32,    BASE_HEX,   VALS(ndps_colorant_set_enum),   0x0,
          "Colorant Set", HFILL }},

        { &hf_ndps_card_enum_time,
        { "Cardinal, Enum, or Time",    "ndps.ndps_card_enum_time",
          FT_UINT32,    BASE_HEX,   VALS(ndps_card_enum_time_enum),   0x0,
          "Cardinal, Enum, or Time", HFILL }},

        { &hf_ndps_attrs_arg,
        { "List Attribute Operation",    "ndps.ndps_attrs_arg",
          FT_UINT32,    BASE_HEX,   VALS(ndps_attrs_arg_enum),   0x0,
          "List Attribute Operation", HFILL }},
        
        { &hf_ndps_context,
        { "Context",    "ndps.context",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Context", HFILL }},

        { &hf_ndps_filter,
        { "Filter Type",    "ndps.ndps_filter",
          FT_UINT32,    BASE_HEX,   VALS(ndps_filter_enum),   0x0,
          "Filter Type", HFILL }},

        { &hf_ndps_item_filter,
        { "Filter Item Operation",    "ndps.ndps_filter_item",
          FT_UINT32,    BASE_HEX,   VALS(ndps_filter_item_enum),   0x0,
          "Filter Item Operation", HFILL }},
        
        { &hf_ndps_substring_match,
        { "Substring Match",    "ndps.ndps_substring_match",
          FT_UINT32,    BASE_HEX,   VALS(ndps_match_criteria_enum),   0x0,
          "Substring Match", HFILL }},

        { &hf_ndps_time_limit,
        { "Time Limit",    "ndps.ndps_time_limit",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Time Limit", HFILL }},

        { &hf_ndps_count_limit,
        { "Count Limit",    "ndps.ndps_count_limit",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Count Limit", HFILL }},

        { &hf_ndps_operator,
        { "Operator Type",    "ndps.ndps_operator",
          FT_UINT32,    BASE_DEC,   VALS(ndps_operator_enum),   0x0,
          "Operator Type", HFILL }},

        { &hf_ndps_password,
        { "Password",    "ndps.password",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Password", HFILL }},
        
        { &hf_ndps_retrieve_restrictions,
        { "Retrieve Restrictions",    "ndps.ndps_ret_restrict",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Retrieve Restrictions", HFILL }},
    
        { &hf_bind_security,
        { "Bind Security Options",    "ndps.ndps_bind_security",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Bind Security Options", HFILL }},
        
        { &hf_ndps_max_items,
        { "Maximum Items in List",    "ndps.ndps_max_items",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Maximum Items in List", HFILL }},

        { &hf_ndps_status_flags,
        { "Status Flag",    "ndps.ndps_status_flags",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Status Flag", HFILL }},

        { &hf_ndps_resource_list_type,
        { "Resource Type",    "ndps.ndps_resource_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_resource_type_enum),   0x0,
          "Resource Type", HFILL }},

        { &hf_os_type,
        { "OS Type",    "ndps.os_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_os_type_enum),   0x0,
          "OS Type", HFILL }},

        { &hf_ndps_printer_type,
        { "Printer Type",    "ndps.prn_type",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer Type", HFILL }},

        { &hf_ndps_printer_manuf,
        { "Printer Manufacturer",    "ndps.prn_manuf",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer Manufacturer", HFILL }},

        { &hf_ndps_inf_file_name,
        { "INF File Name",    "ndps.inf_file_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "INF File Name", HFILL }},

        { &hf_ndps_vendor_dir,
        { "Vendor Directory",    "ndps.vendor_dir",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Vendor Directory", HFILL }},

        { &hf_banner_type,
        { "Banner Type",    "ndps.banner_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_banner_type_enum),   0x0,
          "Banner Type", HFILL }},

        { &hf_font_type,
        { "Font Type",    "ndps.font_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_font_type_enum),   0x0,
          "Font Type", HFILL }},

        { &hf_printer_id,
        { "Printer ID",    "ndps.printer_id",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Printer ID", HFILL }},

        { &hf_ndps_font_name,
        { "Font Name",    "ndps.font_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Font Name", HFILL }},

        { &hf_ndps_return_code,
        { "Return Code",    "ndps.ret_code",
          FT_UINT32,    BASE_HEX,   VALS(ndps_error_types),   0x0,
          "Return Code", HFILL }},

        { &hf_ndps_banner_name,
        { "Banner Name",    "ndps.banner_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Banner Name", HFILL }},

        { &hf_font_type_name,
        { "Font Type Name",    "ndps.font_type_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Font Type Name", HFILL }},

        { &hf_font_file_name,
        { "Font File Name",    "ndps.font_file_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Font File Name", HFILL }},

        { &hf_ndps_prn_file_name,
        { "Printer File Name",    "ndps.print_file_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer File Name", HFILL }},

        { &hf_ndps_prn_dir_name,
        { "Printer Directory Name",    "ndps.print_dir_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer Directory Name", HFILL }},

        { &hf_ndps_def_file_name,
        { "Printer Definition Name",    "ndps.print_def_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Printer Definition Name", HFILL }},

        { &hf_ndps_win31_items,
        { "Windows 31 Keys",    "ndps.win31_keys",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Windows 31 Keys", HFILL }},

        { &hf_ndps_win95_items,
        { "Windows 95 Keys",    "ndps.win95_keys",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Windows 95 Keys", HFILL }},

        { &hf_ndps_windows_key,
        { "Windows Key",    "ndps.windows_key",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Windows Key", HFILL }},

        { &hf_archive_type,
        { "Archive Type",    "ndps.archive_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_archive_enum),   0x0,
          "Archive Type", HFILL }},

        { &hf_archive_file_size,
        { "Archive File Size",    "ndps.archive_size",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Archive File Size", HFILL }},

        { &hf_ndps_segment_overlap,
          { "Segment overlap",	"ndps.segment.overlap", FT_BOOLEAN, BASE_NONE,
    		NULL, 0x0, "Segment overlaps with other segments", HFILL }},
    
        { &hf_ndps_segment_overlap_conflict,
          { "Conflicting data in segment overlap", "ndps.segment.overlap.conflict",
    	FT_BOOLEAN, BASE_NONE,
    		NULL, 0x0, "Overlapping segments contained conflicting data", HFILL }},
    
        { &hf_ndps_segment_multiple_tails,
          { "Multiple tail segments found", "ndps.segment.multipletails",
    	FT_BOOLEAN, BASE_NONE,
    		NULL, 0x0, "Several tails were found when desegmenting the packet", HFILL }},
    
        { &hf_ndps_segment_too_long_segment,
          { "Segment too long",	"ndps.segment.toolongsegment", FT_BOOLEAN, BASE_NONE,
    		NULL, 0x0, "Segment contained data past end of packet", HFILL }},
    
        { &hf_ndps_segment_error,
          {"Desegmentation error",	"ndps.segment.error", FT_FRAMENUM, BASE_NONE,
    		NULL, 0x0, "Desegmentation error due to illegal segments", HFILL }},
    
        { &hf_ndps_segment,
          { "NDPS Fragment",		"ndps.fragment", FT_FRAMENUM, BASE_NONE,
    		NULL, 0x0, "NDPS Fragment", HFILL }},
    
        { &hf_ndps_segments,
          { "NDPS Fragments",	"ndps.fragments", FT_NONE, BASE_NONE,
    		NULL, 0x0, "NDPS Fragments", HFILL }},

        { &hf_ndps_data,
          { "[Data]",	"ndps.data", FT_NONE, BASE_NONE,
    		NULL, 0x0, "[Data]", HFILL }},

        { &hf_get_status_flag,
        { "Get Status Flag",    "ndps.get_status_flags",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Get Status Flag", HFILL }},
        
        { &hf_res_type,
        { "Resource Type",    "ndps.res_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_res_type_enum),   0x0,
          "Resource Type", HFILL }},
              
        { &hf_file_timestamp,
        { "File Time Stamp",    "ndps.file_time_stamp",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "File Time Stamp", HFILL }},

        { &hf_print_arg,
        { "Print Type",    "ndps.print_arg",
          FT_UINT32,    BASE_DEC,   VALS(ndps_print_arg_enum),   0x0,
          "Print Type", HFILL }},

        { &hf_sub_complete,
          { "Submission Complete?",	"ndps.sub_complete", FT_BOOLEAN, BASE_NONE,
    		NULL, 0x0, "Submission Complete?", HFILL }},

        { &hf_doc_content,
        { "Document Content",    "ndps.doc_content",
          FT_UINT32,    BASE_DEC,   VALS(ndps_doc_content_enum),   0x0,
          "Document Content", HFILL }},

        { &hf_ndps_doc_name,
        { "Document Name",    "ndps.doc_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Document Name", HFILL }},

        { &hf_local_id,
        { "Local ID",    "ndps.local_id",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Local ID", HFILL }},

        { &hf_ndps_included_doc,
        { "Included Document",    "ndps.included_doc",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Included Document", HFILL }},

        { &hf_ndps_ref_name,
        { "Referenced Document Name",    "ndps.ref_doc_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Referenced Document Name", HFILL }},

        { &hf_interrupt_job_type,
        { "Interrupt Job Identifier",    "ndps.interrupt_job_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_interrupt_job_enum),   0x0,
          "Interrupt Job Identifier", HFILL }},

        { &hf_pause_job_type,
        { "Pause Job Identifier",    "ndps.pause_job_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_pause_job_enum),   0x0,
          "Pause Job Identifier", HFILL }},

        { &hf_ndps_force,
        { "Force?",    "ndps.ndps_force",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Force?", HFILL }},

        { &hf_resubmit_op_type,
        { "Resubmit Operation Type",    "ndps.resubmit_op_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_resubmit_op_enum),   0x0,
          "Resubmit Operation Type", HFILL }},

        { &hf_shutdown_type,
        { "Shutdown Type",    "ndps.shutdown_type",
          FT_UINT32,    BASE_DEC,   VALS(ndps_shutdown_enum),   0x0,
          "Shutdown Type", HFILL }},

        { &hf_ndps_supplier_flag,
        { "Supplier Data?",    "ndps.supplier_flag",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Supplier Data?", HFILL }},

        { &hf_ndps_language_flag,
        { "Language Data?",    "ndps.language_flag",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Language Data?", HFILL }},

        { &hf_ndps_method_flag,
        { "Method Data?",    "ndps.method_flag",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Method Data?", HFILL }},
        
        { &hf_ndps_delivery_address_flag,
        { "Delivery Address Data?",    "ndps.delivery_flag",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Delivery Address Data?", HFILL }},
       
        { &hf_ndps_list_profiles_type,
        { "List Profiles Type",    "ndps.ndps_list_profiles_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_attrs_arg_enum),   0x0,
          "List Profiles Type", HFILL }},
    
        { &hf_ndps_list_profiles_choice_type,
        { "List Profiles Choice Type",    "ndps.ndps_list_profiles_choice_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_list_profiles_choice_enum),   0x0,
          "List Profiles Choice Type", HFILL }},
        
        { &hf_ndps_list_profiles_result_type,
        { "List Profiles Result Type",    "ndps.ndps_list_profiles_result_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_list_profiles_result_enum),   0x0,
          "List Profiles Result Type", HFILL }},
        
        { &hf_ndps_integer_type_flag,
        { "Integer Type Flag",    "ndps.ndps_integer_type_flag",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Integer Type Flag", HFILL }},
        
        { &hf_ndps_integer_type_value,
        { "Integer Type Value",    "ndps.ndps_integer_type_value",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "Integer Type Value", HFILL }},
        
        { &hf_ndps_continuation_option,
        { "Continuation Option",    "ndps.ndps_continuation_option",
          FT_BYTES,    BASE_HEX,   NULL,   0x0,
          "Continuation Option", HFILL }},

        { &hf_ndps_ds_info_type,
        { "DS Info Type",    "ndps.ndps_ds_info_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_ds_info_enum),   0x0,
          "DS Info Type", HFILL }},

        { &hf_ndps_guid,
        { "GUID",    "ndps.guid",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "GUID", HFILL }},

        { &hf_ndps_list_services_type,
        { "Services Type",    "ndps.ndps_list_services_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_list_services_enum),   0x0,
          "Services Type", HFILL }},

        { &hf_ndps_item_bytes,
        { "Item Ptr",    "ndps.item_bytes",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Item Ptr", HFILL }},

        { &hf_ndps_certified,
        { "Certified",    "ndps.certified",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Certified", HFILL }},

        { &hf_ndps_attribute_set,
        { "Attribute Set",    "ndps.attribute_set",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Attribute Set", HFILL }},

        { &hf_ndps_data_item_type,
        { "Item Type",    "ndps.ndps_data_item_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_data_item_enum),   0x0,
          "Item Type", HFILL }},

        { &hf_info_int,
        { "Integer Value",    "ndps.info_int",
          FT_UINT8,    BASE_HEX,   NULL,   0x0,
          "Integer Value", HFILL }},

        { &hf_info_int16,
        { "16 Bit Integer Value",    "ndps.info_int16",
          FT_UINT16,    BASE_HEX,   NULL,   0x0,
          "16 Bit Integer Value", HFILL }},

        { &hf_info_int32,
        { "32 Bit Integer Value",    "ndps.info_int32",
          FT_UINT32,    BASE_HEX,   NULL,   0x0,
          "32 Bit Integer Value", HFILL }},

        { &hf_info_boolean,
        { "Boolean Value",    "ndps.info_boolean",
          FT_BOOLEAN,    BASE_NONE,   NULL,   0x0,
          "Boolean Value", HFILL }},

        { &hf_info_string,
        { "String Value",    "ndps.info_string",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "String Value", HFILL }},

        { &hf_info_bytes,
        { "Byte Value",    "ndps.info_bytes",
          FT_BYTES,    BASE_NONE,   NULL,   0x0,
          "Byte Value", HFILL }},
       
        { &hf_ndps_list_local_servers_type,
        { "Server Type",    "ndps.ndps_list_local_server_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_list_local_servers_enum),   0x0,
          "Server Type", HFILL }},

        { &hf_ndps_registry_name,
        { "Registry Name",    "ndps.registry_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Registry Name", HFILL }},

        { &hf_ndps_client_server_type,
        { "Client/Server Type",    "ndps.ndps_client_server_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_client_server_enum),   0x0,
          "Client/Server Type", HFILL }},

        { &hf_ndps_session_type,
        { "Session Type",    "ndps.ndps_session_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_session_type_enum),   0x0,
          "Session Type", HFILL }},

        { &hf_time,
        { "Time",    "ndps.time",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Time", HFILL }},

        { &hf_ndps_supplier_name,
        { "Supplier Name",    "ndps.supplier_name",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Supplier Name", HFILL }},

        { &hf_ndps_message,
        { "Message",    "ndps.message",
          FT_STRING,    BASE_NONE,   NULL,   0x0,
          "Message", HFILL }},

        { &hf_delivery_method_type,
        { "Delivery Method Type",    "ndps.delivery_method_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_delivery_method_enum),   0x0,
          "Delivery Method Type", HFILL }},

        { &hf_ndps_get_session_type,
        { "Session Type",    "ndps.ndps_get_session_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_get_session_type_enum),   0x0,
          "Session Type", HFILL }},

        { &hf_packet_count,
        { "Packet Count",    "ndps.packet_count",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Packet Count", HFILL }},

        { &hf_last_packet_flag,
        { "Last Packet Flag",    "ndps.last_packet_flag",
          FT_UINT32,    BASE_DEC,   NULL,   0x0,
          "Last Packet Flag", HFILL }},

        { &hf_ndps_get_resman_session_type,
        { "Session Type",    "ndps.ndps_get_resman_session_type",
          FT_UINT32,    BASE_HEX,   VALS(ndps_get_resman_session_type_enum),   0x0,
          "Session Type", HFILL }},

        { &hf_problem_type,
        { "Problem Type",    "ndps.ndps_get_resman_session_type",
          FT_UINT32,    BASE_HEX,   VALS(problem_type_enum),   0x0,
          "Problem Type", HFILL }},
    };

	static gint *ett[] = {
		&ett_ndps,
		&ett_ndps_segments,
		&ett_ndps_segment,
	};
	module_t *ndps_module;
	
	proto_ndps = proto_register_protocol("Novell Distributed Print System", "NDPS", "ndps");
	proto_register_field_array(proto_ndps, hf_ndps, array_length(hf_ndps));
	proto_register_subtree_array(ett, array_length(ett));

	ndps_module = prefs_register_protocol(proto_ndps, NULL);
	prefs_register_bool_preference(ndps_module, "desegment_tcp",
	    "Desegment all NDPS messages spanning multiple TCP segments",
	    "Whether the NDPS dissector should desegment all messages spanning multiple TCP segments",
	    &ndps_desegment);
	prefs_register_bool_preference(ndps_module, "desegment_spx",
	    "Desegment all NDPS messages spanning multiple SPX packets",
	    "Whether the NDPS dissector should desegment all messages spanning multiple SPX packets",
	    &ndps_defragment);

	register_init_routine(&ndps_init_protocol);
	register_postseq_cleanup_routine(&ndps_postseq_cleanup);
}

void
proto_reg_handoff_ndps(void)
{
	dissector_handle_t ndps_handle, ndps_tcp_handle;

	ndps_handle = create_dissector_handle(dissect_ndps_ipx, proto_ndps);
	ndps_tcp_handle = create_dissector_handle(dissect_ndps_tcp, proto_ndps);
	
	dissector_add("spx.socket", SPX_SOCKET_PA, ndps_handle);
	dissector_add("spx.socket", SPX_SOCKET_BROKER, ndps_handle);
	dissector_add("spx.socket", SPX_SOCKET_SRS, ndps_handle);
	dissector_add("spx.socket", SPX_SOCKET_ENS, ndps_handle);
	dissector_add("spx.socket", SPX_SOCKET_RMS, ndps_handle);
	dissector_add("spx.socket", SPX_SOCKET_NOTIFY_LISTENER, ndps_handle);
	dissector_add("tcp.port", TCP_PORT_PA, ndps_tcp_handle);
	dissector_add("tcp.port", TCP_PORT_BROKER, ndps_tcp_handle);
	dissector_add("tcp.port", TCP_PORT_SRS, ndps_tcp_handle);
	dissector_add("tcp.port", TCP_PORT_ENS, ndps_tcp_handle);
	dissector_add("tcp.port", TCP_PORT_RMS, ndps_tcp_handle);
	dissector_add("tcp.port", TCP_PORT_NOTIFY_LISTENER, ndps_tcp_handle);
	ndps_data_handle = find_dissector("data");
}
