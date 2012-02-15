// Message.cpp
// Implements the Message class.

#include "Message.h"

#include "CycException.h"
#include "Communicator.h"
#include "FacilityModel.h"
#include "MarketModel.h"
#include "InstModel.h"
#include "GenericResource.h"
#include "Logger.h"
#include "BookKeeper.h"

#include <iostream>

// initialize static variables
int Message::nextTransID_ = 1;

std::string Message::outputDir_ = "/output/transactions";

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Message::Message(Communicator* sender) {
  MLOG(LEV_DEBUG4) << "Message " << this << " created.";
  dead_ = false;
  dir_ = UP_MSG;
  sender_ = sender;
  recipient_ = NULL;
  current_owner_ = NULL;
  path_stack_ = vector<Communicator*>();
  current_owner_ = sender;

  trans_.supplier = NULL;
  trans_.requester = NULL;
  trans_.is_offer = NULL;
  trans_.resource = NULL;
  trans_.minfrac = 0;
  trans_.price = 0;

  setRealParticipant(sender);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Message::Message(Communicator* sender, Communicator* receiver) {
  MLOG(LEV_DEBUG4) << "Message " << this << " created.";
  dead_ = false;
  dir_ = UP_MSG;
  sender_ = sender;
  recipient_ = receiver;
  current_owner_ = NULL;

  trans_.supplier = NULL;
  trans_.requester = NULL;
  trans_.is_offer = NULL;
  trans_.resource = NULL;
  trans_.minfrac = 0;
  trans_.price = 0;

  setRealParticipant(sender);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Message::Message(Communicator* sender, Communicator* receiver,
                 Transaction thisTrans) {
  MLOG(LEV_DEBUG4) << "Message " << this << " created.";
  dead_ = false;
  dir_ = UP_MSG;
  trans_ = thisTrans;
  sender_ = sender;
  recipient_ = receiver;
  current_owner_ = NULL;
  setResource(thisTrans.resource);

  if (trans_.is_offer) {
    // if this message is an offer, the sender is the supplier
    setSupplier(dynamic_cast<Model*>(sender_));
  } else if (!trans_.is_offer) {
    // if this message is a request, the sender is the requester
    setRequester(dynamic_cast<Model*>(sender_));
  }

  setRealParticipant(sender);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Message::~Message() {
  MLOG(LEV_DEBUG4) << "Message " << this << " deleted.";
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::setRealParticipant(Communicator* who) {
  Model* model = NULL;
  model = dynamic_cast<Model*>(who);
  if (model != NULL) {model->setIsTemplate(false);}
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::printTrans() {
  std::cout << "Transaction info (via Message):" << std::endl <<
    "    Requester ID: " << trans_.requester->ID() << std::endl <<
    "    Supplier ID: " << trans_.supplier->ID() << std::endl <<
    "    Price: "  << trans_.price << std::endl;
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
msg_ptr Message::clone() {
  CLOG(LEV_DEBUG3) << "Message " << this << "was cloned.";

  msg_ptr new_msg(new Message(*this));
  new_msg->setResource(resource());
  return new_msg;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::sendOn() {
  if (dead_) {return;}

  validateForSend();
  msg_ptr me = msg_ptr(this);

  if (dir_ == DOWN_MSG) {
    path_stack_.back()->untrackMessage(me);
    path_stack_.pop_back();
  } else {
    path_stack_.back()->trackMessage(me);
  }

  Communicator* next_stop = path_stack_.back();
  setRealParticipant(next_stop);
  current_owner_ = next_stop;

  CLOG(LEV_DEBUG1) << "Message " << this << " going to model"
                   << " ID=" << dynamic_cast<Model*>(next_stop)->ID();

  next_stop->receiveMessage(me);

  CLOG(LEV_DEBUG1) << "Message " << this << " returned from model"
                   << " ID=" << dynamic_cast<Model*>(next_stop)->ID();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::kill() {
  CLOG(LEV_DEBUG3) << "Message " << this << "was killed.";
  dead_ = true;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::validateForSend() {
  int next_stop_index = -1;
  bool receiver_specified = false;
  Communicator* next_stop;

  if (dir_ == UP_MSG) {
    receiver_specified = (path_stack_.size() > 0);
    next_stop_index = path_stack_.size() - 1;
  } else if (dir_ == DOWN_MSG) {
    receiver_specified = (path_stack_.size() > 1);
    next_stop_index = path_stack_.size() - 2;
  }

  if (!receiver_specified) {
    string err_msg = "Can't send the message: next dest is unspecified.";
    throw CycMessageException(err_msg);
  }

  next_stop = path_stack_[next_stop_index];
  if (next_stop == current_owner_) {
    string err_msg = "Message receiver and sender are the same.";
    throw CycMessageException(err_msg);
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::setNextDest(Communicator* next_stop) {
  if (dir_ == UP_MSG) {
    CLOG(LEV_DEBUG4) << "Message " << this << " next-stop set to model ID="
                     << dynamic_cast<Model*>(next_stop)->ID();
    if (path_stack_.size() == 0) {
      path_stack_.push_back(sender_);
    }
    path_stack_.push_back(next_stop);
    return;
  }
  CLOG(LEV_DEBUG4) << "Message " << this
                   << " next-stop set attempt ignored to model ID="
                   << dynamic_cast<Model*>(next_stop)->ID();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::reverseDirection() {
  if (DOWN_MSG == dir_) {
    CLOG(LEV_DEBUG4) << "Message " << this << "direction flipped from 'down' to 'up'.";
    dir_ = UP_MSG; 
  } else {
    CLOG(LEV_DEBUG4) << "Message " << this << "direction flipped from 'up' to 'down'.";
  	dir_ = DOWN_MSG;
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MessageDir Message::dir() const {
  return dir_;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Message::setDir(MessageDir newDir) {
  CLOG(LEV_DEBUG4) << "Message " << this << "manually changed to "
                   << newDir << ".";

  dir_ = newDir;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Communicator* Message::market() {
  MarketModel* market = MarketModel::marketForCommod(trans_.commod);
  return market;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Communicator* Message::recipient() const {
  if (recipient_ == NULL) {
    string err_msg = "Uninitilized message recipient.";
    throw CycMessageException(err_msg);
  }

  return recipient_;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Model* Message::supplier() const {
  if (trans_.supplier == NULL) {
    string err_msg = "Uninitilized message supplier.";
    throw CycMessageException(err_msg);
  }

  return trans_.supplier;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Model* Message::requester() const {
  if (trans_.requester == NULL) {
    string err_msg = "Uninitilized message requester.";
    throw CycMessageException(err_msg);
  }

  return trans_.requester;
}

void Message::approveTransfer() {
  msg_ptr me = msg_ptr(this);

  Model* req = requester();
  Model* sup = supplier();
  vector<rsrc_ptr> manifest = sup->removeResource(me);
  req->addResource(me, manifest);

  BI->registerTransaction(nextTransID_++, me, manifest);

  CLOG(LEV_INFO3) << "Material sent from " << sup->ID() << " to " 
                  << req->ID() << ".";

  CLOG(LEV_INFO4) << "Begin material transfer details:";
  for (int i = 0; i < manifest.size(); i++) {
    manifest.at(i)->print();
  }
  CLOG(LEV_INFO4) << "End material transfer details.";

}

