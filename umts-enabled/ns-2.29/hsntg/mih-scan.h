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

#ifndef mih_scan_h
#define mih_scan_h

#include "mih.h"
#include "mih-types.h"
#include "mih-user.h"

class MIHScan {
  friend class MIHAgent;
 public:
  /** 
   * Create information about a local request
   * @param mih The MIHF
   * @param user The MIH User
   * @param req The scan request
   */
  MIHScan (MIHAgent *mih, MIHUserAgent *user, struct mih_scan_request_s *req);

  /**
   * Create information about a remote request
   * @param mih The MIHF
   * @param session The session by which the command was received
   * @param req The message containing the request
   */
  MIHScan (MIHAgent *mih, int session, u_int16_t transactionid, struct mih_scan_req *req);

  /**
   * Process scan response
   * @param macAddr The interface responding to the scan
   * @param rsp The media specific response
   */
  bool process_scan_response (int macAddr, void *rsp, int size);

  
 protected:
  
  /**
   * Indicate if the request is from local user
   * @return true if the request is local
   */
  inline bool isLocalRequest () { return lreq_ != NULL;}
  
  /**
   * Return the MIH User
   * @return The MIH User
   */
  MIHUserAgent * getUser ();

  /**
   * Return the session id for remote communication
   * @return The session id for remote communication
   */
  int getSession ();

  /**
   * Return the transaction id for the request
   * @return The transaction id for the request
   */
  u_int16_t getTransaction ();

  /**
   * Return the scan response
   * @return the scan response
   */
  struct mih_scan_rsp * getResponse ();

 private:
  /**
   * MIHF
   */
  MIHAgent *mih_;

  /**
   * Local user requesting scan
   */
  MIHUserAgent *user_;

  /**
   * The local scan request
   */
  struct mih_scan_request_s *lreq_; 

  /**
   * The remote scan request
   */
  struct mih_scan_req * rreq_;

  /** 
   * Session ID for remote request
   */
  int sessionid_;

  /**
   * The transaction id to match request/response
   */
  u_int16_t transactionid_;

  /**
   * The scan response
   */
  struct mih_scan_rsp *rsp_;

};

#endif
