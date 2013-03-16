/*
 * Include file for Media Independant Handover types
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
 * <BR>
 * <BR>
 * </PRE><P>
 */

#ifndef mih_types_h
#define mih_types_h

/*
 * Defines the MIH_LINK_SAP datastructure
 */

// Define the different types of events
enum mih_event_t {
  MIH_LINK_UP=0,
  MIH_LINK_DOWN,
  MIH_LINK_GOING_DOWN,
  MIH_LINK_DETECTED,
  MIH_LINK_PARAMETERS_REPORT,
  MIH_LINK_EVENT_ROLLBACK,
  MIH_LINK_SDU_TRANSMIT_STATUS,
  MIH_LINK_HANDOVER_IMMINENT,
  MIH_LINK_HANDOVER_COMPLETE,

  // MIH_SCAN_RESPONSE, - doesn't seem to be in the new spec

  LINK_LAST_EVENT //last event of the list
};

// Define command list
enum link_command_t {
  LINK_CAPABILITY_DISCOVER=0,
  LINK_EVENT_DISCOVER,
  LINK_EVENT_SUBSCRIBE,
  LINK_EVENT_UNSUBSCRIBE,
  MIH_LINK_CONFIGURE_THRESHOLD,
  MIH_LINK_GET_PARAMETERS
};

#define link_event_t mih_event_t //draft 3.0 defines same id

/*
 * Define the possible status of an interface
 */
enum interface_status_t {
  LINK_STATUS_UP=1,
  LINK_STATUS_DOWN,
  LINK_STATUS_GOING_DOWN,
  LINK_STATUS_HANDOVER_IMMINENT,
  LINK_STATUS_HANDOVER_COMPLETE,
  LINK_STATUS_UNKNOWN
};

/*
 * Define the link types (also called network type)
 */
enum link_type_t {
  LINK_GENERIC=0, /* Not defined in RADIUS NAS-Port-Type */
  LINK_ETHERNET=15,
  LINK_WIRELESS_OTHER=18,
  LINK_802_11=19,
  LINK_CDMA2000=22,
  LINK_UMTS=23,
  LINK_CDMA2000_HRDP=24,
  LINK_802_16=27, 
  LINK_802_20=28,
  LINK_802_22=29,

  LINK_LAST_TYPE
};

/*
 * Define Mobility management protocol supported
 */
enum mobility_mgmt_support_t {
  MOBILE_IPV4_FA=0,
  MOBILE_IPV4_NOFA,
  IPV6,
  IPV6_DHCP
};


/*
 * Define link identifier structure
 */
struct link_identifier_t {
  link_type_t type;
  int macMobileTerminal;  // mac address of Mobile Terminal
  int macPoA;             // mac address of PoA /*optional*/
  //To keep here until we make sure we don't need pointer to mac 
  //Mac *eventSource;       //Origination point from where the event is generated
};


/*
 * Define data structure for each link event
 */
class Mac;
class Node;

//LINK UP
struct link_up_t {
  link_identifier_t linkIdentifier;     // New Link Identifier
  // Optional information
  int macOldAccessRouter;  // is this contained in the eventSource structure?
  int macNewAccessRouter;          //MAC address of new PoA (AP). Equivalent to bss_id
  bool ipRenewal;
  int mobilityManagement;
};

//LINK DOWN
enum link_down_reason_t {
  LD_RC_EXPLICIT_DISCONNECT=1,
  LD_RC_PACKET_TIMEOUT=2,
  LD_RC_FAIL_NORESOURCE=3,
  LD_RC_FAIL_NO_BROADCAST=4,
  LD_RC_FAIL_AUTHENTICATION=5,
  LD_RC_FAIL_BILLING=6
  /* 7-127 are reserved */
  /* 128-255 are vendor specific */
};

struct link_down_t {
  link_identifier_t linkIdentifier;     // New Link Identifier
  int macOldAccessRouter;               // is this contained in the eventSource structure?
  link_down_reason_t reason;            //Reason for why the link went down
};

//LINK GOING DOWN
enum link_going_down_reason_t {
  LGD_RC_EXPLICIT_DISCONNECT=0,
  LGD_RC_LINK_PARAM_DEGRADING=1,
  LGD_RC_LOW_POWER=2,
  LGD_RC_NO_RESOURCE=3
  /* 4-127 are reserved */
  /* 128-255 are vendor specific */
};

struct link_going_down_t {
  link_identifier_t linkIdentifier;     // New Link Identifier
  unsigned int timeInterval;    //Time interval in which the link is expected to go down (in ms) 0-65535
  short int confidenceLevel;     //Confidence level for link to go down within the specified time 1-100 (percent)
  link_going_down_reason_t reasonCode; 
  unsigned int uniqueEventIdentifier; //To be used for callback event
};

//LINK ROLLBACK
struct link_rollback_t {
  link_identifier_t linkIdentifier;
  unsigned int uniqueEventIdentifier; //Used to identify the event which needs to be rolled back
};

//LINK DETECTED
struct link_detected_t {
  link_identifier_t linkIdentifier;     // New Link Identifier
  bool mihCapability;    //0-MIH not supported, 1-MIH supported
};

// LINK PARAMETERS REPORT and LINK GET PARAMETERS
//First octet is for link type, second octet is parameter
enum parameter_type_t { //defines the list of parameters that can be changed
  //Generic
  LINK_GEN_LINK_SPEED=0x0,
  LINK_GEN_BER,
  LINK_GEN_FRAME_LOSS,
  LINK_GEN_SIGNAL_STRENGH, 
  LINK_GEN_SINR,
  LINK_GEN_PACKET_DELAY,
  LINK_GEN_PACKET_JITTER,
  //we define our own too
  LINK_GEN_RX_DATA_THROUGHPUT,
  LINK_GEN_RX_TRAFFIC_THROUGHPUT,
  LINK_GEN_TX_DATA_THROUGHPUT,
  LINK_GEN_TX_TRAFFIC_THROUGHPUT,
  //GPRS
  LINK_GPRS_BLER=0x0,
  LINK_GPRS_RXLEVNCELL,
};

/** link parameter is splitted into 2 octets. First one defines link type
 * second one defines parameter
 */
struct link_parameter_type_s {
  link_type_t link_type;
  parameter_type_t parameter_type;
};

union param_value {
  bool data_b;
  char data_c;
  int data_i;
  long data_l;
  float data_f;
  double data_d;
};

struct link_parameter_s {
  link_parameter_type_s parameter; //The name of the parameter that changed
  union param_value old_value;
  union param_value new_value;
};

#define MAX_NB_PARAM 20 //can be changed to fit requirements
struct link_parameters_report_t {
  link_identifier_t linkIdentifier;  // current link identifier
  //int macAccessRouter; // mac address of current access router (if any)
  int numLinkParameters;
  struct link_parameter_s linkParameterList[MAX_NB_PARAM];
};

/* Stucture for the link_configure_threshold */
struct link_param_th {
  link_parameter_type_s parameter;
  union param_value initActionTh;
  union param_value rollbackActionTh;
  union param_value exectActionTh;
};

struct link_param_th_status {
  link_parameter_type_s parameter;
  int status; //0-reject, 1-success
};

/* Structure for link_get_parameter */
struct link_get_param_s {
  link_parameter_type_s parameter;
  union param_value value;
};

// LINK SDU TRANSMIT STATUS
enum transmit_status_t {
  SDU_TRANSMIT_SUCCESS=0,
  SDU_TRANSMIT_FAILURE=1
};

struct link_sdu_transmit_status_t {
  link_identifier_t linkIdentifier;
  unsigned int packetIdentifier; // 0-65535
  transmit_status_t transmitStatus; // 0: success, 1: failure
};


//LINK HANDOVER IMMINENT
struct link_handover_imminent_t {
  link_identifier_t linkIdentifier;
  link_identifier_t newLinkIdentifier;
  int macOldAccessRouter; //MAC address of old Access Router (if any)
  int macNewAccessRouter; //MAC address of new Access Router (if any)
};

//LINK HANDOVER COMPLETE
struct link_handover_complete_t {
  link_identifier_t linkIdentifier;
  int macOldAccessRouter; //MAC address of old Access Router (if any)
  int macNewAccessRouter; //MAC address of new Access Router (if any)
};

/**
 * MIH Scan request and response
 * Draft 2.0 defines that request contains Media specific 
 * scan command. Does it defeat the purpose of being media independent?
 * So for now, we have a void* parameter for command and response
 * in order to contain media specific structure.
 */

//MIH SCAN REQUEST
struct mih_scan_req_entry_s {
  link_identifier_t linkIdentifier;
  int commandSize; //size in bytes of media specific command
  void *command;            //Data containing scan parameter
};

struct mih_scan_request_s {
  int nbElement;
  mih_scan_req_entry_s *requestSet;
};

//MIH SCAN RESPONSE
struct mih_scan_poa_s {
  int macPoA;
  bool mihCapability;
};

struct mih_scan_rsp_entry_s {
  link_identifier_t linkIdentifier;
  //int nbPoA; //number of PoA found
  //mih_scan_poa_s listPoA[8]; //list of PoA with their MIH capability
  void *result;
};

struct mih_scan_response_s {
  int nbElement; 
  mih_scan_rsp_entry_s *result; //results for different link
};

/* 
 * Defines data for parameter request and configuration
 * This data structure is used to configure and request 
 * configuration of the link.link_parameter_config_t
 */
#define PARAMETER_UNKNOWN_VALUE -1


struct link_parameter_config_t {
  link_type_t type;   //The type of interface
  double bandwidth;   //The bandwidth
  double ber;         //Bit error rate
  double delay;       //link delay
  int macPoA;         //mac address of PoA
};

#define link_event_list u_int32_t
#define link_command_list u_int32_t
#define link_event_status u_int32_t

struct capability_list_s {
  link_event_list events;
  link_command_list commands;
};

// Define structures for MIH_SAP

//MIH_Configure
enum mih_operation_mode_t {
  NORMAL_MODE=0,
  POWER_SAVING,
  POWER_DOWN
};

//list the possible elements that can be configured
//used for bit location in request
enum mih_link_config_param_t {
  CONFIG_OPERATION_MODE=0,
  CONFIG_DISABLE_TRANSMITTER,
  CONFIG_LINK_ID,
  CONFIG_CURRENT_ADDRESS,
  CONFIG_SUSPEND_DRIVER,
  CONFIG_QOS_PARAMETER_LIST
};

enum mih_qos_type_t {
  MIH_QOS_THROUGHPUT=0,
  MIH_QOS_PER,
  MIH_QOS_NO_COS,
  MIH_QOS_MIN_PACKET_DELAY,
  MIH_QOS_MEAN_PACKET_DELAY,
  MIH_QOS_MAX_PACKET_DELAY,
  MIH_QOS_PACKET_DELAY_JITTER,
  MIH_QOS_PACKET_LOSS,
  /* 8-255 reserved */
  MIH_QOS_LAST
};

//Link QoS Parameters element
struct mih_qos_param_s {
  mih_qos_type_t type;
  u_int32_t value;  //for MIH_QOS_PER and MIN_QOS_NO_COS it is only 2 bytes
};

struct mih_configure_req_s {
  u_int32_t information; //bitmap of which element is being configured
  mih_operation_mode_t operationMode;
  bool disableTransmitter;
  link_identifier_t linkIdentifier;
  int currentAddress;
  bool suspendDriver;
  //list of QoS Parameters
  int nbParam;
  struct mih_qos_param_s *linkQoSParameters;
};

struct mih_config_response_s {
  mih_link_config_param_t type;
  bool resultCode;
};

struct mih_configure_conf_s {
  struct link_identifier_t linkId;
  //list of configuration parameter and result
  int nbParam;
  struct mih_config_response_s *responseSet;
  //Status of operation
  bool status;
};

enum mih_link_status_options {
  NETWORK_TYPES=0,
  DEVICE_INFO,
  OPERATION_MODE,
  NETWORK_ID,
  CHANNEL_ID,
  CHANNEL_QUALITY,
  LINK_SPEED,
  BATTERY_LEVEL,
  QOS_INFO,
  /*media dependent tbd*/
};

struct mih_get_status_entry_s {
  u_int32_t deviceInfo; //not used
  mih_operation_mode_t operationMode; 
  struct link_identifier_t linkId;
  u_int32_t channelId;
  u_int32_t battery_level;
  //link QoS Parameters...
  int nbParam;
  mih_qos_param_s *linkQoSParameters;
};


#define MAX_INTERFACE_PER_NODE 4 //modify if necessary
//for the request, we only need a bitmap of the requested information
//response per link
struct mih_get_status_s {
  u_int32_t information; //which information is included (bitmap)
  int nbInterface; //number of interfaces' status
  struct mih_get_status_entry_s status [MAX_INTERFACE_PER_NODE];
};


struct mih_cap_disc_conf_s {
  struct link_identifier_t linkIdentifier;
  struct capability_list_s capability;
};

struct mih_handover_initiate_req_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t linkIdentifier;
  link_identifier_t *suggestedNewLinkList;
  int *suggestedNewPoAList;
  u_char handoverMode;
  u_int16_t oldLinkAction;
  u_int32_t QueryResourceList;
  bool QueryResourceFlag;
};

struct mih_handover_initiate_ind_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t linkIdentifier;
  link_identifier_t *suggestedNewLinkList;
  int *suggestedNewPoAList;
  u_char handoverMode;
  u_int16_t oldLinkAction;
  u_int32_t QueryResourceList;
};

struct mih_handover_initiate_res_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  bool handoverAck;
  link_identifier_t *preferredLinkList;
  int *preferredPoAList;
  u_char abortReason;
  u_int32_t availableResourceList;
  u_int32_t QueryResourceList;

};

struct mih_handover_initiate_confirm_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  bool handoverAck;
  link_identifier_t *preferredLinkList;
  int *preferredPoAList;
  u_char abortReason;
  u_int32_t availableResourceList;

};

struct mih_handover_prepare_req_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  u_int32_t QueryResourceList;
};

struct mih_handover_prepare_ind_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  u_int32_t QueryResourceList;

};

struct mih_handover_prepare_res_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  u_char resourceStatus;
  u_int32_t availableResourceList;

};

struct mih_handover_prepare_confirm_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  u_char resourceStatus;
  u_int32_t availableResourceList;

};

struct mih_handover_commit_req_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  link_identifier_t newLinkIdentifier;
  int macNewPoA;
  u_int16_t oldLinkAction;
};

struct mih_handover_commit_ind_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  link_identifier_t newLinkIdentifier;
  int macNewPoA;
  u_int16_t oldLinkAction;
};

struct mih_handover_commit_res_s {
  u_int32_t destinationIdentifier;
  u_int32_t sourceIdentifier;
  link_identifier_t currentLinkIdentifier;
  u_int16_t oldLinkAction;
  u_char handoverStatus;
};

struct mih_handover_commit_confirm_s {
  u_int32_t destinationIdentifier;
  u_int32_t sourceIdentifier;
  link_identifier_t currentLinkIdentifier;
  u_int16_t oldLinkAction;
  u_char handoverStatus;
};

struct mih_handover_complete_ind_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
};

struct mih_handover_complete_res_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
};

struct mih_handover_complete_confirm_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
};

struct mih_network_address_info_req_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  int targetPoAIdentifier;
  u_int32_t ipConfigurationMethods;
  int currentDHCPServerAddress;
  int currentAccessRouterAddress;
  int currentFAAdress;
};

struct mih_network_address_info_res_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  int newPoAIdentifier;
  int DHCPServerAddress;
  int FAAddress;
  u_int16_t ipConfigurationMethods;
  u_int32_t accessRouterAddress;
  u_char resultCode;
};

struct mih_network_address_info_ind_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  int homeAddress;
  int CoA;
  int oldAccessRouterAddress;
  int oldFAAddress;
};

struct mih_network_address_info_confirm_s {
  u_int32_t sourceIdentifier;
  u_int32_t destinationIdentifier;
  link_identifier_t currentLinkIdentifier;
  int homeAddress;
  int FAAddress;
  int accessRouterAddress;
  int networkAddressInformation;
};

#endif
