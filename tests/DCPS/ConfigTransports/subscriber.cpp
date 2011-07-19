// -*- C++ -*-
// ============================================================================
/**
 *  @file   subscriber.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#include "DDSTEST.h"

#include "DataReaderListener.h"

#include "tests/DCPS/FooType4/FooDefTypeSupportImpl.h"

#include "common.h"
#include "../common/TestSupport.h"
#include "../common/TestException.h"

#include "dds/DdsDcpsSubscriptionC.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/transport/framework/EntryExit.h"

#ifdef ACE_AS_STATIC_LIBS
#include "dds/DCPS/transport/tcp/Tcp.h"
#endif


DDS::Subscriber_ptr
create_configured_subscriber (DDS::DomainParticipant_ptr dp)
{

  // Create the subscriber
  DDS::Subscriber_var sub =
          dp->create_subscriber (SUBSCRIBER_QOS_DEFAULT,
                                 DDS::SubscriberListener::_nil (),
                                 OpenDDS::DCPS::DEFAULT_STATUS_MASK);
  TEST_CHECK (!CORBA::is_nil (sub.in ()));

  // If there is a ini file-based configuration name initialize
  // the transport configuration for the corresponding Entity
  TEST_CHECK (!configuration_str.empty ());
  if (configuration_str != "none" && entity_str == "pubsub")
    {
      OpenDDS::DCPS::TransportRegistry::instance ()->bind_config (configuration_str, sub.in ());
    }

  return sub._retn ();
}

DDS::DataReader_ptr
create_configured_reader (DDS::Subscriber_ptr sub, DDS::DataReaderListener_ptr drl)
{
  // Create the data readers
  DDS::DataReaderQos dr_qos;
  sub->get_default_datareader_qos (dr_qos);

  dr_qos.durability.kind = durability_kind;
  dr_qos.liveliness.kind = liveliness_kind;
  dr_qos.liveliness.lease_duration = LEASE_DURATION;
  dr_qos.reliability.kind = reliability_kind;

  // When collocation doesn't matter we choose a topic name that will not match
  // the publisher's topic name
  std::string topicname ((collocation_str == "none") ? MY_OTHER_TOPIC : MY_SAME_TOPIC);

  DDS::TopicDescription_var description =
          sub->get_participant ()->lookup_topicdescription (topicname.c_str ());
  TEST_CHECK (!CORBA::is_nil (description.in ()));

  DDS::DataReader_var rd (sub->create_datareader (description.in (),
                                                  dr_qos,
                                                  drl,
                                                  ::OpenDDS::DCPS::DEFAULT_STATUS_MASK));

  // Initialize the transport configuration for the appropriate entity
  TEST_CHECK (!configuration_str.empty ());
  if (configuration_str != "none" && entity_str == "rw")
    {
      OpenDDS::DCPS::TransportRegistry::instance ()->bind_config (configuration_str,
                                                                  rd.in ());
    }

  return rd._retn ();
}

bool
assert_subscription_matched (DDS::DataReaderListener_ptr drl)
{
  // Assert if pub/sub made a match ...
  DataReaderListenerImpl* drl_servant =
          dynamic_cast<DataReaderListenerImpl*> (drl);

  // there is an error if we matched when not compatible (or vice-versa)
  return !compatible || drl_servant->subscription_matched ();
}

int
ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
  try
    {
      DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs (argc, argv);

      // let the Service_Participant (in above line) strip out -DCPSxxx parameters
      // and then get application specific parameters.
      ::parse_args (argc, argv);

      DDS::DomainParticipant_var dp (create_configured_participant (dpf.in()));
      TEST_CHECK (!CORBA::is_nil (dp.in ()));

      DDS::Topic_var topic (create_configured_topic (dp.in ()));
      TEST_CHECK (!CORBA::is_nil (topic.in ()));

      // Create the subscriber
      DDS::Subscriber_var sub (create_configured_subscriber (dp.in ()));
      TEST_CHECK (!CORBA::is_nil (sub.in ()));

      DDS::DataReaderListener_var drl (new DataReaderListenerImpl ());
      DDS::DataReader_var dr (create_configured_reader (sub.in (), drl.in ()));
      TEST_CHECK (!CORBA::is_nil (dr.in ()));

      // Wait for things to settle ?!
      ACE_OS::sleep (test_duration);

      // Assert effective configuration properties
      ACE_ERROR ((LM_INFO,
                  ACE_TEXT ("(%P|%t) Validating if the entity '%C' effective protocol is '%C'\n"),
                  entity_str.c_str (),
                  protocol_str.c_str ()));

      TEST_CHECK (::DDS_TEST::supports (dr.in (), protocol_str));

      // Clean up subscriber objects
      sub->delete_contained_entities ();
      dp->delete_subscriber (sub.in ());
      dp->delete_topic (topic.in ());
      dpf->delete_participant (dp.in ());

      TheServiceParticipant->shutdown ();

      // there is an error if we matched when not compatible (or vice-versa)
      if (!assert_subscription_matched (drl.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                             ACE_TEXT ("(%P|%t) Expected subscription_matched to be %C, but it wasn't.")
                             ACE_TEXT ("durability_kind=%s, liveliness_kind=%s, liveliness_duration=%s, ")
                             ACE_TEXT ("reliability_kind=%s\n"),
                             (compatible) ? "true" : "false",
                             durability_kind_str.c_str (),
                             liveliness_kind_str.c_str (),
                             LEASE_DURATION_STR.c_str (),
                             reliability_kind_str.c_str ()),
                            1);
        }

    }
  catch (const TestException&)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                         ACE_TEXT ("(%P|%t) TestException caught in main.cpp. ")), 1);
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in main.cpp:");
      return 1;
    }

  ACE_ERROR_RETURN ((LM_INFO,
                     ACE_TEXT ("(%P|%t) done.\n")), 0);
}