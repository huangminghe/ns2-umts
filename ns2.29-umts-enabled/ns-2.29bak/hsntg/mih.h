/*
 * Include file for Media Independant Handover 
 *
 * This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 * and makes no guarantees, expressed or implied, about its quality,
 * reliability, or any other characteristic.
 * <BR>
 * We would appreciate acknowledgement if the software is used.
 * <BR>
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 * @version 1.0
 * <P><PRE>
 * VERSION CONTROL<BR>
 * -------------------------------------------------------------------------<BR>
 * Name  - YYYY/MM/DD - VERSION - ACTION<BR>
 * rouil - 2005/05/18 - 1.0     - Source created<BR>
 * rouil - 2006/10/30 - 1.1     - Updated to match P802.21/D02.00
 * <BR>
 * <BR>
 * </PRE><P>
 */

#ifndef mih_h
#define mih_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
//#include "node.h"
#include "address.h"
#include "mih-types.h"
#include <iostream>
#include <vector>
#include <map>
#include "nd.h"

//class Mac;
#include "mac.h"

/*
 * Define Media Independent Handover Protocol
 */
#define HDR_MIH(p)    ((struct hdr_mih*)(p)->access(hdr_mih::offset_))

// type of service
enum mih_service_id_t {
  MIH_SYSTEM_SERVICE = 1,
  MIH_EVENT_SERVICE,
  MIH_COMMAND_SERVICE,
  MIH_INFORMATION_SERVICE
};

// possible opcode in the message
enum mih_opcode_t {
  MIH_REQUEST = 1,
  MIH_RESPONSE,
  MIH_INDICATION
};

// identified the message received
enum mih_message_id {
  
  MIH_MSG_CAPABILITY_DISCOVER = 1,
  MIH_MSG_EVENT_SUBSCRIBE,
  MIH_MSG_EVENT_UNSUBSCRIBE,

  MIH_MSG_LINK_UP,
  MIH_MSG_LINK_DOWN,
  MIH_MSG_LINK_GOING_DOWN,
  MIH_MSG_LINK_DETECTED,
  MIH_MSG_LINK_PARAMETERS_REPORT,
  MIH_MSG_LINK_EVENT_ROLLBACK,
  MIH_MSG_LINK_HANDOVER_IMMINENT,
  MIH_MSG_LINK_HANDOVER_COMPLETE,

  /*MIH_MSG_SDU_TRANSMIT_STATUS,*/ //missing from the draft???

  MIH_MSG_GET_STATUS,
  MIH_MSG_SWITCH,
  MIH_MSG_CONFIGURE,
  MIH_MSG_SCAN,
  MIH_MSG_HANDOVER_INITIATE,
  MIH_MSG_HANDOVER_PREPARE,
  MIH_MSG_HANDOVER_COMMIT,
  MIH_MSG_HANDOVER_COMPLETE,
  MIH_MSG_NETWORK_ADDRESS_INFORMATION,

  MIH_MSG_GET_INFORMATION,

  /* 21+: reserved */
  /* Where are the following definitions in the draft? */
  MIH_MSG_REGISTRATION,
  MIH_MSG_DEREGISTRATION
};

// common header for all MIH messages
struct hdr_mih_common {
  u_char protocol_version:4;
  u_char ack_req:1;
  u_char ack_rsp:1;
  u_int16_t reserved: 10;
  u_char mih_service_id:4;
  u_char mih_opcode:2;
  u_int16_t mih_aid: 10;
  u_int16_t transaction_id;
  u_int16_t load_len;
  //MIHF Variable header. Optional
  u_int32_t mihf_id; //source identifier
  u_int32_t session_id;
};


/*
 * Messages for system management service category (section 8.5.1)
 */

//The MIH capability discover request 
struct mih_cap_disc_req {
  //optional TLV fields
  u_int32_t supported_event_list;
  u_int32_t supported_command_list;
  u_int32_t supported_transport_list;
  u_char    supported_query_type_list; 
};

//The MIH capability discover response
struct mih_cap_disc_res {
  //optional TLV fields
  u_int32_t supported_event_list;
  u_int32_t supported_command_list;
  u_int32_t supported_transport_list;
  u_char    supported_query_type_list; 
};

//MIH registration request 
struct mih_reg_req {
  //u_int32_t sourceID; //present in the header
  u_int32_t destID; 
  u_char req_code; //0-registration, 1-reRegistration
  //Standard defines that Session ID is created
  //at registration but there is no information about
  //it in the messages
  u_int16_t msb_session; //first 2 octets of session ID
};

//MIH registration response
struct mih_reg_rsp {
  //u_int32_t sourceID; //present in header
  u_int32_t destID; 
  u_int32_t validity;
  u_char reg_result; //1 success, 0 failure
  u_char failure_code; //values not defined yet
  //Standard defines that Session ID is created
  //at registration but there is no information about
  //it in the messages
  u_int16_t lsb_session; //last 2 octets of session ID
};

//MIH deregistration request 
struct mih_dereg_req {
  //u_int32_t sourceID; //present in header
  u_int32_t destID; 
};

//MIH deregistration response
struct mih_dereg_rsp {
  //u_int32_t sourceID; //present in header
  u_int32_t destID; 
  u_char result_code; //1 success, 0 failure
  u_char failure_code; //values not defined yet
};

/*
 * MIH Event Service Packet Format (section 8.5.2)
 */

//MIH Link UP indication
struct mih_link_up_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macNewPoA[ETHER_ADDR_LEN];
  u_char macOldAccessRouter[ETHER_ADDR_LEN];
  u_char macNewAccessRouter[ETHER_ADDR_LEN];
  u_char ipRenewalFlag;
  u_int16_t ipConfigurationMethod;
};

//MIH Link DOWN indication
enum mih_proto_link_down_reason {
  MIH_PROTO_RC_EXPLICIT_DISCONNECT = 0,
  MIH_PROTO_RC_PACKET_TIMEOUT,
  MIH_PROTO_RC_FAIL_NORESOURCE
  /* 3-127 are reserved */
  /* 128-255 are vendor specific */
};

struct mih_link_down_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_char macOldAccessRouter[ETHER_ADDR_LEN];
  u_char reasonCode;
};

//MIH Link GOING DOWN indication
struct mih_link_going_down_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macCurrentPoA[ETHER_ADDR_LEN];
  //  u_char macNewPoA[ETHER_ADDR_LEN];
  u_char macOldAccessRouter[ETHER_ADDR_LEN];
  u_char macNewAccessRouter[ETHER_ADDR_LEN];
  u_int16_t timeInterval;
  u_char confidenceLevel;
  u_int16_t uniqueEventIdentifier;
};

//MIH Link Event Rollback indication
struct mih_link_rollback_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macCurrentPoA[ETHER_ADDR_LEN];
  u_int16_t uniqueEventIdentifier;
};

//MIH Link Detected indication
struct mih_link_detected_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_char capability;
};

//MIH Link Parameters Report indication
//TBD: the message length should change depending on the parameter
//     values (ie. char < int..)
//     should also support multiple parameters
struct mih_link_parameters_report_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macAccessRouter[ETHER_ADDR_LEN];
  u_char nbParam;
  struct link_parameter_s linkParameterList[MAX_NB_PARAM];
};

//MIH link handover imminent
struct mih_link_handover_imminent_ind {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t new_link_type;
  u_char new_macMobileTerminal[ETHER_ADDR_LEN];
  u_char new_macPoA[ETHER_ADDR_LEN];
  u_char macOldAccessRouter[ETHER_ADDR_LEN];
  u_char macNewAccessRouter[ETHER_ADDR_LEN];
};

//MIH link handover complete
struct mih_link_handover_complete_ind {
  u_int32_t new_linkType;
  u_char new_macMobileTerminal[ETHER_ADDR_LEN];
  u_char new_macPoA[ETHER_ADDR_LEN];
  u_char macOldAccessRouter[ETHER_ADDR_LEN];
  u_char macNewAccessRouter[ETHER_ADDR_LEN];
};

/* 
 * MIH Command Service Packet (section 8.5.3)
 */

//The MIH Event Subscribe request
struct mih_event_sub_req {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t req_eventList;
};

//The MIH Event Subscribe response
struct mih_event_sub_rsp {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t res_eventList;
};

//The MIH Event Unsubscribe request
struct mih_event_unsub_req {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t req_eventList;
};

//The MIH Event Unsubscribe response
struct mih_event_unsub_rsp {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t res_eventList;
};

//The MIH Get Status request. 
struct mih_get_status_req {
  u_int32_t link_status_parameter_type;
};

//The MIH Get Status response. 
struct mih_get_status_rsp_entry {
  u_int32_t deviceInfo; //not used
  u_char operationMode;
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t channelId;
  u_int32_t channelQuality;
  u_int32_t linkSpeed;
  u_int32_t battery_level;
  u_int32_t linkQoSParameter [MIH_QOS_LAST];  
};

struct mih_get_status_rsp {
  u_int32_t link_status_parameter_type;
  u_char nbInterface;
  mih_get_status_rsp_entry status [MAX_INTERFACE_PER_NODE]; 
};

//The MIH Switch request
struct mih_switch_req {
  u_char handover_mode;
  //new link identifer
  u_int32_t new_linkType;
  u_char new_macMobileTerminal[ETHER_ADDR_LEN];
  u_char new_macPoA[ETHER_ADDR_LEN];
  //old link identifier
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_int32_t oldLinkAction;
};

//The MIH Switch response
struct mih_switch_rsp {
  u_char status;
};

//The MIH Configure request
struct mih_configure_req {
  //list of link parameters and values
  //Not clear in the draft how to put it.
  u_int32_t opMode;
  u_char disableTransmitter;
  u_char macPoA[ETHER_ADDR_LEN]; //LINK ID
  u_char macMobileTerminal[ETHER_ADDR_LEN]; //CURRENT ADDRESS
  u_char suspendDriver;
  //LINK QOS PARAMETER LIST
  int nbParam;
  struct mih_qos_param_s qosParameters[8]; //currently 8 defined
};

//The MIH Configure response
struct mih_configure_rsp {
  //list of link parameters and result codes
  u_char opModeStatus;
  u_char disableTransmitterStatus;
  u_char linkIDStatus; //LINK ID
  u_char currentAddressStatus;
  u_char suspendDriverStatus;
  //LINK QOS PARAMETER LIST
  int nbParam;
  u_char qosParametersStatus[8]; //currently 8 defined
};

//The MIH Scan request
struct mih_scan_req_entry {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN]; 
  //u_char commandSize; //size in byte of the scan commands
  //u_char scanCommand[255]; //enough space to store command
};

struct mih_scan_req {
  u_char nbElement;
  mih_scan_req_entry requestSet[8]; //supports up to 8 links for now
};

//The MIH Scan response
struct mih_scan_rsp_entry {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN]; 
  u_char responseSize; //size in byte of the scan commands
  u_char scanResponse[255]; //enough space to store results
};

struct mih_scan_rsp {
  //PoA list
  u_char nbElement;
  mih_scan_rsp_entry resultSet[8];
};

//The MIH Configure threshold request
struct mih_conf_th_req {
  u_int32_t new_linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  //list of link quality parameters and threshold values
  //currently put a maximum of 8 threshold at once
  u_char nbParam;
  u_int32_t linkParameter [8];
  u_int32_t initiateActionThreshold [8];
  u_int32_t rollbackActionThreshold [8];
  u_int32_t executeActionThreshold [8];
};

//The MIH Configure threshold response
struct mih_conf_th_rsp {
  u_int32_t new_linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char macPoA[ETHER_ADDR_LEN];
  u_char nbParam;
  u_int32_t configureThresholdResultCode [8];
};

//MIH Handover initiate request
struct mih_handover_initiate_req {
  u_int32_t suggestedLinkType;
  u_char suggestedMacMobileTerminal[ETHER_ADDR_LEN];
  u_char suggestedNewPoA[ETHER_ADDR_LEN];
  //u_int32_t suggestedNewPoA;
  u_char handoverMode;      //Make-before-Break or Break-before-Make
  u_int16_t oldLinkAction; //action for the old link
};

//MIH Handover initiate response
struct mih_handover_initiate_res {
  u_char handoverAck;
  u_int32_t preferredLinkType;
  u_char preferredMacMobileTerminal[ETHER_ADDR_LEN];
  u_char preferredPoA[ETHER_ADDR_LEN];
  u_char abortReason; //action for the old link
};

//MIH Handover prepare request
struct mih_handover_prepare_req {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char currentPoA[ETHER_ADDR_LEN];
  u_char queryResourceList;
};

//MIH Handover prepare response
struct mih_handover_prepare_res {
  u_int32_t linkType;
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char preferredPoA[ETHER_ADDR_LEN];
  u_char resourceRetention;
  u_int32_t queryResourceList; //value of resource requested. Not well defined
};

//MIH Handover commit request
struct mih_handover_commit_req {
  u_int32_t linkType;
  u_char newMacMobileTerminal[ETHER_ADDR_LEN];
  u_int16_t oldLinkAction; //action for the old link
};

//MIH Handover commit response
struct mih_handover_commit_res {
  u_int16_t oldLinkAction; //action for the old link
};

//MIH Handover complete request
struct mih_handover_complete_req {
  u_char handoverStatus;
};

//MIH Handover complete response
struct mih_handover_complete_res {
  u_char resourceRetention;
};


#define IPv6_ADDRESS_SIZE 16 //IPv6_ADDR_SIZE defined in nd.h
//MIH network address information request
struct mih_network_address_info_req {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  u_char targetPoAIdentifier[ETHER_ADDR_LEN];
  u_int32_t ipConfigurationMethods;
  u_char currentDHCPServerAddress[IPv6_ADDRESS_SIZE];
  u_char macMobileTerminal[ETHER_ADDR_LEN];
  u_char servingAccessRouter [IPv6_ADDRESS_SIZE];
  u_char currentFAAddress[IPv6_ADDRESS_SIZE];
};

//MIH network address information request
struct mih_network_address_info_res {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  u_char newPoAIdentifier[ETHER_ADDR_LEN];
  u_int16_t ipConfigurationMethods;
  u_char DHCPServerAddress[IPv6_ADDRESS_SIZE];
  u_char FAAddress[IPv6_ADDRESS_SIZE];
  u_char accessRouter [IPv6_ADDRESS_SIZE];
};

struct mih_network_address_info_ind {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  u_char homeAddress[IPv6_ADDRESS_SIZE];
  u_char CoA[IPv6_ADDRESS_SIZE];
  u_char oldAccessRouterAddress[IPv6_ADDRESS_SIZE];
  u_char oldFAAddress[IPv6_ADDRESS_SIZE];

};

struct mih_network_address_info_confirm {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  u_char homeAddress[IPv6_ADDRESS_SIZE];
  u_char FAAddress[IPv6_ADDRESS_SIZE];
  u_char accessRouterAddress[IPv6_ADDRESS_SIZE];
  u_char networkAddressInformation[IPv6_ADDRESS_SIZE];
};

/* 
 * MIH Information Service Packet (section 8.5.4)
 */

//MIH Get Information request
struct mih_get_information_req {
  u_char queryType;
  //Query parameters
};

struct mih_get_information_rsp {
  u_char queryType;
  //Query response
  //status
};

// Create the packet with a maximum size
struct hdr_mih {
  struct hdr_mih_common hdr;
  union payload_t { //this is to reserve the maximum space used by messages
    struct mih_cap_disc_req mih_cap_disc_req_;
    struct mih_cap_disc_res mih_cap_disc_res_;
    struct mih_reg_req mih_reg_req_;
    struct mih_reg_rsp mih_reg_rsp_;
    struct mih_dereg_req mih_dereg_req_;
    struct mih_dereg_rsp mih_dereg_rsp_;
    struct mih_link_up_ind mih_link_up_ind_;
    struct mih_link_down_ind mih_link_down_ind_;
    struct mih_link_going_down_ind mih_link_going_down_ind_;
    struct mih_link_detected_ind mih_link_detected_ind_;
    struct mih_link_rollback_ind mih_link_rollback_ind_;
    struct mih_link_parameters_report_ind mih_link_parameters_report_ind_;
    struct mih_link_handover_imminent_ind mih_link_handover_imminent_ind_;
    struct mih_link_handover_complete_ind mih_link_handover_complete_ind_;
    struct mih_event_sub_req mih_event_sub_req_;
    struct mih_event_sub_rsp mih_event_sub_rsp_;
    struct mih_event_unsub_req mih_event_unsub_req_;
    struct mih_event_unsub_rsp mih_event_unsub_rsp_;
    struct mih_get_status_req mih_get_status_req_;
    struct mih_get_status_rsp mih_get_status_rsp_;
    struct mih_switch_req mih_switch_req_;
    struct mih_switch_rsp mih_switch_rsp_;
    struct mih_configure_req mih_configure_req_;
    struct mih_configure_rsp mih_configure_rsp_;
    struct mih_scan_req mih_scan_req_;
    struct mih_scan_rsp mih_scan_rsp_;
    struct mih_conf_th_req mih_conf_th_req_;
    struct mih_conf_th_rsp mih_conf_th_rsp_;
    struct mih_handover_initiate_req mih_handover_initiate_req_; 
    struct mih_handover_initiate_res mih_handover_initiate_res_;
    struct mih_handover_prepare_req mih_handover_prepare_req_; 
    struct mih_handover_prepare_res mih_handover_prepare_res_;
    struct mih_handover_commit_req mih_handover_commit_req_; 
    struct mih_handover_commit_res mih_handover_commit_res_;
    struct mih_handover_complete_req mih_handover_complete_req_; 
    struct mih_handover_complete_res mih_handover_complete_res_;
    struct mih_network_address_info_req mih_network_address_info_req_;
    struct mih_network_address_info_res mih_network_address_info_res_;
    struct mih_get_information_req mih_get_information_req_;
    struct mih_get_information_rsp mih_get_information_rsp_;
  }payload;

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_mih* access(Packet* p) {
    return (hdr_mih*) p->access(offset_);
  }
};

/** END MIH protocol **/

class MIHUserAgent;
class MIHAgent;
class MIHInterfaceInfo;
class MIHScan;

#define transport_list u_int32_t
#define is_query_type_list u_char
#define MIH_CAP_DELAY 0.1


/** Define structure to store MAC information */
/** Structure to store remote MIHF information */
struct mihf_info {
  int id; //unique MIHF-ID
  int macAddr; //Mac address of PoS
  int address; //l3 address of PoS

  link_event_list supported_events;
  link_command_list supported_commands;
  transport_list transport;
  is_query_type_list query_type;
  
  struct session_info *session;

  Mac *local_mac; //local MAC where we received discovery response
};

/** Structure for session information */
struct session_info {
  int id; //unique session id
  int macAddr; //local MAC to use to send messages on this session
  struct mihf_info *mihf; //pointer to MIHF-ID
  int lastTID; //the TID of last packet received. To check duplicate
};

/** Class to store pending request **/
class MIHRequestTimer;

struct mih_pending_req {
  MIHUserAgent *user; //entity requesting 
  Packet *pkt; //the packet to send
  int tid;
  bool waitAck; //indicates if we are still waiting for ACK
  bool waitRsp; //indicates if we are still waiting for Response
  MIHRequestTimer *timer; //timer for acknoledgement
  double timeout; //timeout value (depending on packet type)
  int retx; //number of retransmission
  session_info *session; //session this packet belongs
  bool l2transport; //true if l2 is used, else l3
  Mac *mac; //interface node to use
};

class MIHRequestTimer : public TimerHandler {
 public:
        MIHRequestTimer(MIHAgent *a, int tid) : TimerHandler() { a_ = a; tid_ = tid;}
 protected:
	void expire(Event *);
        MIHAgent *a_;
	int tid_;
};

/**
 * Defines MIB for the MIH 
 */
class MIH_MIB {
public:
	MIH_MIB(MIHAgent *parent);

	u_int32_t supported_events_;
	u_int32_t supported_commands_;
	u_int32_t supported_transport_;
	u_int32_t supported_query_type_;
	
	double es_timeout_;
	double cs_timeout_;
	double is_timeout_;
	int retryLimit_; //for Acknoledged packets
};

/*
 * Define the MIH Function agent
 */
class MIHAgent : public Agent {
  friend class GetStatusTimer;
  friend class MIHRequestTimer;
  friend class MIHScan;
 public:
  MIHAgent();
  int command(int argc, const char*const* argv);
  void recv(Packet*, Handler*);
  void recv_event (link_event_t, void*);
  
  MIHInterfaceInfo* get_interface (int);
  MIHInterfaceInfo** get_interfaces ();
  inline int get_nb_interfaces () { return (int)ifaces_.size();}
  vector <Mac*> get_local_mac ();
  Mac *get_mac (int macAddr);
  bool is_local_mac (int macAddr);

  /**
   * Register this MIH User to receive System Management messages
   * @param user The MIH User
   */
  void register_management_user (MIHUserAgent *user);

  
  //commands that can be used by upper protocols
  void link_configure (Mac *, link_parameter_config_t*);
  
  //MIH Commands
  /**
   * Broadcast capability discover message
   * @param user The MIH User
   * @param mac The MAC to use to broadcast
   */
  void send_cap_disc_req (MIHUserAgent *user, Mac *mac);
  
  /**
   * Send a register request to the given MIHF
   * @param user The MIH User
   * @param mihfId The MIHF ID
   */
  void send_reg_request (MIHUserAgent *user, int mihfId);

  /**
   * Send a registration response to the given MIHF
   * @param user The MIH User
   * @param mihf The remote MIHF
   * @param reg_result The registration result
   * @param failure_code If it fails, this is the reason
   * @param validity If it succeeded, how long does it last
   * @param tid Transaction ID to use
   */
  void send_reg_response (MIHUserAgent *user, int mihfId, u_char reg_result, u_char failure_code, u_int32_t validity, u_int16_t tid);

  /**
   * Send a deregister request to the given MIHF
   * @param user The MIH User
   * @param mihfId The MIHF ID
   */
  void send_dereg_request (MIHUserAgent *user, int mihfId);

  /**
   * Send a deregistration respose to the given MIHF
   * @param user The MIH User
   * @param mihf The remote MIHF
   * @param dereg_result The registration result
   * @param failure_code If it fails, this is the reason
   * @param tid Transaction ID to use
   */
  void send_dereg_response (MIHUserAgent *user, int mihfId, u_char dereg_result, u_char failure_code, u_int16_t tid);

  /**
   * Discover MIH Capability of link layer
   * @param user The MIH user
   * @param mihfid The ID of destination MIHF
   * @param linkId The Link identifier
   */
  mih_cap_disc_conf_s *mih_capability_discover (MIHUserAgent *user, u_int32_t mihfid, link_identifier_t *linkId); 

  /**
   * Subscribe to events on the given link
   * @param user The MIH user
   * @param mihfid The destination MIH ID
   * @param linkId The Link identifier
   * @param events The events to subscribe
   * @return The status if the link is local or NULL
   */
  link_event_status *mih_event_subscribe (MIHUserAgent *user, u_int32_t mihfid, link_identifier_t linkId, link_event_list events);
  

  /**
   * Unsubscribe to events on the given link
   * @param user The MIH user
   * @param mihfId The destination MIH ID 
   * @param linkId The Link identifier
   * @param events The events to unsubscribe
   * @return The status if the link is local or NULL
   */
  link_event_status *mih_event_unsubscribe (MIHUserAgent *user, u_int32_t mihfid, link_identifier_t linkId, link_event_list events);

  /**
   * Return the link status on the given MIHF
   * @param user The MIH user
   * @param mihfid The ID of destination MIHF
   * @param status_request Set of information requested
   * @return The status if the MIH is local or NULL
   */
  mih_get_status_s *mih_get_status (MIHUserAgent *user, u_int32_t mihfid, u_int32_t status_request); //request link status

  
  /**
   * Switch an active session from one link to another
   * @param mode The handover mode
   * @param oldLink Old link identifier
   * @param newLink New link identifier
   * @param oldLinkAction Action for old link
   */
  void mih_switch (u_char mode, link_identifier_t oldLink, link_identifier_t newLink, u_char oldLinkAction);

  /**
   * Configure the mac using the given configuration
   * @param user The MIH User
   * @param mihfid The destination MIHF
   * @param config The configuration to apply
   */
  mih_config_response_s * mih_configure_link (MIHUserAgent *user, u_int32_t mihfid, mih_configure_req_s *config);

  /**
   * Request scanning for one or more links
   * @param user The MIH User
   * @param mihfid The ID of destination MIHF
   * @param req The scan request
   */
  void mih_scan (MIHUserAgent *user, u_int32_t mihfid, struct mih_scan_request_s *req); 

  void send_handover_initiate (Mac *, int); //mih handover initiate request
  void send_handover_initiate_response (Packet *, int); //mih handover initiate response
  void send_handover_prepare (Mac *, int); //mih handover prepare request 
  void send_handover_prepare_response (Packet *, int);//mih handover prepare response
  void send_handover_commit (Mac *, int); //mih handover commit request
  void send_handover_commit_response (Packet *, int);//mih handover commit response
  void send_handover_complete (Mac *, int); //mih handover complete request
  void send_handover_complete_response (Packet *, int);//mih handover complete response

  /**
   * Send an Network Address Infomration request
   * @param user The MIH User
   * @param mihf Target MIHF
   * @param link Current link identifier
   * @param targetPoA Mac address of target PoA
   */
  void mih_network_address_information (MIHUserAgent *user, int mihfid, link_identifier_t link, int targetPoA);

  /**
   * Request information from Information Server
   * @param user The MIH User
   * To complete
   */
  void mih_get_information (MIHUserAgent *user);

  /**
   * Process the scan response from the lower layer
   * @param mac The lower layer
   * @param rsp The media dependant scan response
   */
  void process_scan_response (Mac *mac, void *rsp, int size);

  /**
   * Return the ID of this MIHF 
   * @return the MIHF ID
   */
  u_int32_t getID ();

  /**
   * Convert the MAC address from 48 bits to int
   * @param mac The MAC address as 48bits
   * @return the MAC address as int
   */
  int eth_to_int (u_char mac[ETHER_ADDR_LEN]);

  /**
   * Convert the MAC address from int to 48bits 
   * @param the MAC address as int
   * @param array to store converted value
   */
  void int_to_eth (int macInt, u_char mac[ETHER_ADDR_LEN]);
  
  /**
   * Return the MIHF ID for the given session
   * @param sessionid The session ID
   * @return the MIHF ID for the session
   */
  u_int32_t getMihfId (u_int32_t sessionid);

 protected:
  Node *node_;                 //my node
  int default_port_;           //default port number for MIH
  double poll_delay_;          //time to handle poll requests
  u_int32_t mihfId_;            //unique MIHF ID
  MIH_MIB         mihmib_;

  vector <MIHInterfaceInfo*> ifaces_;     //list of the interfaces
  
  vector <mihf_info*> peer_mihfs_;
  vector <session_info*> sessions_;
  map <int, mih_pending_req*> pending_req_;   //list of pending request messages
  vector <MIHUserAgent *> management_users_; //list of MIH User to receive System Management messages
  vector <MIHScan *> scan_req_;

  //processing events
  /**
   * Process MIH_LINK_UP
   * @param e The Link UP event
   */
  void process_link_up (link_up_t *e);

  /**
   * Process MIH_LINK_DOWN
   * @param e The Link DOWN event
   */
  void process_link_down (link_down_t *e);

  /**
   * Process MIH_LINK_GOING_DOWN
   * @param e The Link Going Down event
   */
  void process_link_going_down (link_going_down_t *e);

  /**
   * Process MIH_LINK_DETECTED
   * @param e The Link Detected event
   */
  void process_link_detected (link_detected_t *e);

  /**
   * Process MIH_LINK_PARAMETERS_REPORT
   * @param e The Link Parameters report event
   */
  void process_link_parameters_report (link_parameters_report_t *e);

  /**
   * Process MIH_LINK_ROLLBACK
   * @param e The Link Rollback event
   */
  void process_link_rollback (link_rollback_t *e);

  /**
   * Process MIH_LINK_HANDOVER_IMMINENT
   * @param e The Link Handover imminent event
   */
  void process_link_handover_imminent (link_handover_imminent_t *e);

  /**
   * Process MIH_LINK_HANDOVER_COMPLETE
   * @param e The Link Handover complete event
   */
  void process_link_handover_complete (link_handover_complete_t *e);

  /**
   * Execute a link scan
   * @param linkId The link identifier
   * @param cmd The scan command
   */
  void link_scan (link_identifier_t linkId, void *cmd);

  //MIH protocol 
  void recv_cap_disc_req (Packet*);
  void recv_cap_disc_rsp (Packet*);
  void recv_reg_req (Packet*);
  void recv_reg_rsp (Packet*);
  void recv_dereg_req (Packet*);
  void recv_dereg_rsp (Packet*);
  void recv_link_up_ind (Packet*);
  void recv_link_down_ind (Packet*);
  void recv_link_going_down_ind (Packet*);
  void recv_link_detected_ind (Packet*);
  void recv_link_rollback_ind (Packet*);
  void recv_link_parameters_report_ind (Packet*);
  void recv_link_handover_imminent_ind (Packet*);
  void recv_link_handover_complete_ind (Packet*);
  void recv_event_sub_req (Packet*);
  void recv_event_sub_rsp (Packet*);
  void recv_event_unsub_req (Packet*);
  void recv_event_unsub_rsp (Packet*);
  void recv_mih_get_status_req (Packet*);
  void recv_mih_get_status_res (Packet*);
  void recv_mih_switch_req (Packet*);
  void recv_mih_switch_res (Packet*);
  void recv_mih_configure_req (Packet*);
  void recv_mih_configure_res (Packet*);
  void recv_mih_scan_req (Packet*);
  void recv_mih_scan_res (Packet*);
  void recv_mih_conf_th_req (Packet*);
  void recv_mih_conf_th_res (Packet*);
  void recv_handover_initiate_req (Packet*);
  void recv_handover_initiate_res (Packet*);
  void recv_handover_prepare_req (Packet*);
  void recv_handover_prepare_res (Packet*);
  void recv_handover_commit_req (Packet*);
  void recv_handover_commit_res (Packet*);
  void recv_handover_complete_req (Packet*);
  void recv_handover_complete_res (Packet*);
  void recv_network_address_info_req (Packet*);
  void recv_network_address_info_res (Packet*);

  /**
   * Process a request timeout
   * @param tid the Transaction ID for the request
   */
  void process_request_timer (int);

  /**
   * Send the given packet to the remote MIHF via the given session
   * @param user The MIHUser 
   * @param p The packet to send (with MIH Header filled)
   * @param session The session to remote MIHF
   */
  void send_mih_message (MIHUserAgent *user, Packet *p, session_info *session);

 private:
  /**
   * transaction ID
   */
  int transactionId_;

  /**
   * Return the MAC for the node with the given address
   * @return The MAC for the node with the given address
   */
  Mac * get_mac_for_node (int addr);

  /**
   * Return the MIHF information for the given MIHF ID
   * @param mihfid The MIHF ID
   * @return the MIHF information or NULL
   */
  mihf_info * get_mihf_info (int mihfid);

  /**
   * Return the session for the given session ID
   * @param sessionid The session ID
   * @return the session information or NULL
   */
  session_info * get_session (int sessionid);

  /**
   * Delete the session and clean all registrations
   * @param sessionid The session ID
   */
  void delete_session (int sessionid);

  /**
   * Clear the pending request
   * @param req The request to clear
   */
  void clear_request (mih_pending_req *req);

};

#endif
