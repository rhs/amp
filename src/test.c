#include <amp/engine2.h>
#include <stdio.h>
#include <string.h>

#include "engine/engine-internal.h"

int main(int argc, char **argv)
{
  amp_connection_t *conn = amp_connection();
  amp_open(conn);

  amp_transport(conn);

  for (int i = 0; i < 1; i++)
  {
    amp_session_t *ssn = amp_session(conn);
    amp_begin(ssn);
    for (int j = 0; j < 1; j++)
    {
      amp_link_t *l;
      if (j % 2)
        l = (amp_link_t *) amp_sender(ssn, L"sender");
      else
        l = (amp_link_t *) amp_receiver(ssn, L"receiver");
      for (int k = 0; k < 4; k++)
      {
        amp_delivery(l, strtag("tag0"));
      }
      amp_attach(l);
      amp_link_dump(l);
      amp_delivery_t *d = amp_delivery(l, strtag("tag1"));
      for (int k = 0; k < 16; k++) {
        d = amp_delivery(l, strtag("tag2"));
        amp_settle(d);
      }
      amp_link_dump(l);
    }
  }

  amp_dump(conn);

  amp_destroy((amp_endpoint_t *)conn);
  return 0;
}
