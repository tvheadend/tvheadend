Enter the access control details to secure your system. The first part
of this covers the network details for address-based access to
the system; for example, 192.168.1.0/24 to allow local access only
to 192.168.1.x clients, or 0.0.0.0/0 or empty value for access
from any system.


This works alongside the second part, which is a familiar username/password
combination, so provide these for both an administrator and regular
(day-to-day) user.


**Notes**:


* You may enter a comma-separated list of network prefixes (IPv4/IPv6).
  If you were asked to enter a username and password during installation,
  we'd recommend not using the same details for a user here as it may cause
  unexpected behavior, incorrect permissions etc.
* To allow anonymous access for any account (administrative or regular user) enter
  an asterisk (*) in the username and password fields. ___It is not___
  recommended that you allow anonymous access to the admin account.
* If you plan on accessing Tvheadend over the Internet, make sure you use
  strong credentials and ___do not allow anonymous access at all___.
