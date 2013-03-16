/* This software was developed at the National Institute of Standards and
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
 */

#ifndef CONTENTIONREQUEST_H
#define CONTENTIONREQUEST_H

#include "mac802_16.h"
#include "mac802_16timer.h"

class ContentionSlot;
class ContentionTimer;
class Mac802_16;

class ContentionRequest;
/** Timer for backoff */
class WimaxBackoffTimer : public WimaxTimer {
 public:
  WimaxBackoffTimer(ContentionRequest *c, Mac802_16 *m) : WimaxTimer(m) {c_=c;}
  
  void	handle(Event *e);
  void pause(void);
  void resume(void);
 private:
  ContentionRequest *c_;
}; 


class ContentionRequest;
LIST_HEAD (contentionRequest, ContentionRequest);

/**
 * This class is used to manage contention opportunities
 * supports list
 */
class ContentionRequest
{
  friend class WimaxBackoffTimer;
 public:
  /**
   * Creates a contention slot for the given frame
   * @param s The contention slot 
   * @param p The packet to send
   */
  ContentionRequest (ContentionSlot *s, Packet *p);
  virtual ~ContentionRequest ();
  /**
   * Called when timeout expired
   */
  virtual void expire ();

  /**
   * Start the timeout timer
   */
  void starttimeout();

  /** 
   * Pause the backoff timer
   */
  void pause ();
  
  /**
   * Resume the backoff timer
   */
  void resume ();

  /// Chain element to the list
  inline void insert_entry_head(struct contentionRequest *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  /// Chain element to the list
  inline void insert_entry(ContentionRequest *elem) {
    LIST_INSERT_AFTER(elem, this, link);
  }

  /// Return next element in the chained list
  ContentionRequest* next_entry(void) const { return link.le_next; }

  /// Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

 protected:

  /**
   * The contention slot information
   */
  ContentionSlot *s_;

  /**
   * The backoff timer
   */
  WimaxBackoffTimer *backoff_timer_;

  /**
   * The timeout timer
   */
  ContentionTimer *timeout_timer_;

  /**
   * Type of timer
   */
  timer_id type_; 

  /**
   * Value for timeout
   */
  double timeout_;

  /**
   * The current window size
   */
  int window_;

  /**
   * Number of retry
   */
  int nb_retry_;

  /** 
   * The scheduler to inform about timeout
   */
  Mac802_16 *mac_;

  /**
   * The packet to send when the backoff expires
   */
  Packet *p_;

  /**
   * Pointer to next in the list
   */
  LIST_ENTRY(ContentionRequest) link;
  //LIST_ENTRY(ContentionRequest); //for magic draw
};

/**
 * Class to handle ranging opportunities
 */
class RangingRequest: public ContentionRequest 
{
 public:

  /**
   * Creates a contention slot for the given frame
   * @param frame The frame map 
   */
  RangingRequest (ContentionSlot *s, Packet *p);

  /**
   * Called when timeout expired
   */
  void expire ();

 private:
};


/**
 * Class to handle bandwidth request opportunities
 */
class BwRequest: public ContentionRequest 
{
 public:

  /**
   * Creates a contention slot for the given frame
   * @param frame The frame map 
   */
  BwRequest (ContentionSlot *s, Packet *p, int cid, int length);

  /**
   * Called when timeout expired
   */
  void expire ();

  /**
   * Return the CID for this request
   * @return the CID for this request
   */
  inline int getCID () { return cid_; }

 private:
  /**
   * The CID for the request
   */
  int cid_;

  /**
   * The size in bytes of the bandwidth requested
   */
  int size_;
};

#endif
