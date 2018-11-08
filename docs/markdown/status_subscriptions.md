# Status - Subscriptions

<tvh_include>inc/status_contents</tvh_include>

---

## Overview

This tab shows information about all active subscriptions to Tvheadend.

This is a read-only tab; nothing is configurable.

!['Status - Subscriptions' Tab](static/img/doc/status/subscriptions.png)

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Items

**ID**
: Subscription ID.

**Hostname**
: Hostname/IP address using the subscription.

**Username**
: Username using the subscription - a blank cell indicates the
subscriber didn't supply a username.

**Title**
: Title of the application using the subscription - you will sometimes
see "epggrab" here, this is an internal subscription used by Tvheadend
to grab EPG data.

**Channel**
: The name of the [channel](class/channel) the subscription is using -
if the subscription is streaming a service/mux this cell will be blank.

**Service**
: The service used by the subscription.

**Profile**
: The name of the [profile](class/profile) the subscription is using.

**Start**
: The date (and time) the subscription was started.

**State**
: The status of the subscription

State         | Description
--------------|-------------
Running       | The subscription is active - the stream is being sent.
Idle          | The subscription is idling, waiting for the subscriber.
Testing       | Tvheadend is testing the requested stream to see if it's available - if a subscription stays in this state too long it may indicate a signal issue.

**PID list**
: Input source Program Identification (PIDs) numbers in use by the subscription.

**Descramble**
: The CAID used to descramble the stream.

**Errors**
: Number of errors occurred sending the stream.

**Input**
: The input data rate in kb/s.

**Output**
: The output data rate in kb/s.
