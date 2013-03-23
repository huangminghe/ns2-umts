/* Helper class to handle scan requests
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
 * rouil - 2006/11/30 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#include "mih-scan.h"
#include "mih.h"
#include "mac-802_11.h"
#include "mac-802_11.h"
/** 
 * Create information about a local request
 * @param mih The MIHF
 * @param user The MIH User
 * @param req The scan request
 */
MIHScan::MIHScan (MIHAgent *mih, MIHUserAgent *user, struct mih_scan_request_s *req)
{
  assert (mih);
  assert (user);
  assert (req);
  mih_ = mih;
  user_ = user;
  lreq_ = req;
  //init other non used variables
  rreq_ = NULL;
  sessionid_ = -1;
  //allocate response
  rsp_ = (struct mih_scan_rsp *) malloc (sizeof (struct mih_scan_rsp));
  rsp_->nbElement = 0;
  //execute link commands
  printf ("There are %d elements in the request\n", lreq_->nbElement);
  for (int i = 0 ; i < lreq_->nbElement ; i++) {
    //add somethings
    if(req->requestSet[i].linkIdentifier.type == LINK_802_11){
      struct Mac802_11_scan_request_s *command = (struct Mac802_11_scan_request_s*) malloc (sizeof (struct Mac802_11_scan_request_s));
      command->scanType = SCAN_TYPE_ACTIVE;
      command->action = SCAN_ALL;
      command->nb_channels = 11;
      command->pref_ch = (int*) malloc (11*sizeof (int));
      for (int j=0 ; j <11 ; j++) {
	command->pref_ch[j] = j+1;
      }
      command->minChannelTime = 0.020;
      command->maxChannelTime = 0.060;
      command->probeDelay = 0.002;
      mih_->link_scan (lreq_->requestSet[i].linkIdentifier, command);
    }
    if(req->requestSet[i].linkIdentifier.type == LINK_802_16){
      struct Mac802_16_scan_request_s *command ;
      mih_->link_scan (lreq_->requestSet[i].linkIdentifier, command);
    }
  }
}

/**
 * Create information about a remote request
 * @param mih The MIHF
 * @param session The session by which the command was received
 * @param transactionid The transaction to match request/response
 * @param req The message containing the request
 */
MIHScan::MIHScan (MIHAgent *mih, int session, u_int16_t transactionid, struct mih_scan_req *req)
{
  assert (mih);
  assert (req);
  mih_ = mih;
  sessionid_ = session;
  transactionid_ = transactionid;
  rreq_ = (struct mih_scan_req *) malloc (sizeof (struct mih_scan_req));
  memcpy (rreq_, req, sizeof (struct mih_scan_req));
  //init other non used variables
  lreq_ = NULL;
  user_ = NULL;
  //allocate response
  rsp_ = (struct mih_scan_rsp *) malloc (sizeof (struct mih_scan_rsp));
  rsp_->nbElement = 0;
  //execute link commands
  for (int i = 0 ; i < rreq_->nbElement ; i++) {
    link_identifier_t linkIdentifier;
    linkIdentifier.type = (link_type_t) rreq_->requestSet[i].linkType;
    linkIdentifier.macMobileTerminal = mih_->eth_to_int(rreq_->requestSet[i].macMobileTerminal);
    linkIdentifier.macPoA = mih_->eth_to_int(rreq_->requestSet[i].macPoA);
    if(linkIdentifier.type == LINK_802_11){
      struct Mac802_11_scan_request_s *command = (struct Mac802_11_scan_request_s*) malloc (sizeof (struct Mac802_11_scan_request_s));
      command->scanType = SCAN_TYPE_ACTIVE;
      command->action = SCAN_ALL;
      command->nb_channels = 11;
      command->pref_ch = (int*) malloc (11*sizeof (int));
      for (int j=0 ; j <11 ; j++) {
	command->pref_ch[j] = j+1;
      }
      command->minChannelTime = 0.020;
      command->maxChannelTime = 0.060;
      command->probeDelay = 0.002;
      mih_->link_scan (linkIdentifier, command);
    }
    if(linkIdentifier.type == LINK_802_16){
      struct Mac802_16_scan_request_s *command ;     
      mih_->link_scan (linkIdentifier, command);
    }
  }
}

/**
 * Return the MIH User
 * @return The MIH User
 */
MIHUserAgent * MIHScan::getUser ()
{
  return user_;
}

/**
 * Return the session id for remote communication
 * @return The session id for remote communication
 */
int MIHScan::getSession ()
{
  return sessionid_;
}

/**
 * Return the transaction id for the request
 * @return The transaction id for the request
 */
u_int16_t MIHScan::getTransaction ()
{
  return transactionid_;
}

/**
 * Return the scan response
 * @return the scan response
 */
struct mih_scan_rsp * MIHScan::getResponse ()
{
  return rsp_;
}


/**
 * Process scan response
 * @param macAddr The interface responding to the scan
 * @param rsp The media specific response
 * @return true if all the links completed the requested scanning
 */
bool MIHScan::process_scan_response (int macAddr, void *rsp, int size)
{
  if (isLocalRequest ()) {
    for (int i=0; i < lreq_->nbElement ; i++) {
      if (lreq_->requestSet[i].linkIdentifier.macMobileTerminal == macAddr) {
	//we requested to scan this link
	rsp_->resultSet[rsp_->nbElement].linkType = lreq_->requestSet[i].linkIdentifier.type;
	mih_->int_to_eth (lreq_->requestSet[i].linkIdentifier.macMobileTerminal, rsp_->resultSet[rsp_->nbElement].macMobileTerminal);
	mih_->int_to_eth (lreq_->requestSet[i].linkIdentifier.macPoA, rsp_->resultSet[rsp_->nbElement].macPoA);
	memcpy (rsp_->resultSet[rsp_->nbElement].scanResponse, rsp, size);
	rsp_->resultSet[rsp_->nbElement].responseSize = (u_char)size;
	rsp_->nbElement++;
	break;
      }
    }
    if (rsp_->nbElement == lreq_->nbElement) {
      //scanning done...inform mih
      return true;
    }
  } else {
    for (int i=0; i < rreq_->nbElement ; i++) {
      if (mih_->eth_to_int (rreq_->requestSet[i].macMobileTerminal) == macAddr) {
	//we requested to scan this link
	rsp_->resultSet[rsp_->nbElement].linkType = rreq_->requestSet[i].linkType;
	memcpy (rsp_->resultSet[rsp_->nbElement].macMobileTerminal, rreq_->requestSet[i].macMobileTerminal, ETHER_ADDR_LEN);
	memcpy (rsp_->resultSet[rsp_->nbElement].macPoA, rreq_->requestSet[i].macPoA, ETHER_ADDR_LEN);
	memcpy (rsp_->resultSet[rsp_->nbElement].scanResponse, rsp, size);
	rsp_->resultSet[rsp_->nbElement].responseSize = (u_char)size;
	rsp_->nbElement++;
	break;
      }
    }  
    if (rsp_->nbElement == rreq_->nbElement) {
      //scanning done...inform mih
      return true;
    }
  }
  return false;
}
