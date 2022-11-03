
#include "contiki.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/sdn-statis.h"
#include <stdio.h>


#include "sys/log.h"
#define LOG_MODULE "COPM_SDN"
#define LOG_LEVEL COMP_SDN_LOG_LEVEL

struct tsch_slotframe *sf = NULL;
static int sf_cell_used = 0;
static uint16_t sf_count = 0;
static uint16_t max_sf_cell_used = 0;
/*----------------------------------------------------------------------*/
//PROCESS(cell_used_process, "cell used process");
//AUTOSTART_PROCESSES(&conf_handle_process);
/*----------------------------------------------------------------------*/
/*
PROCESS_THREAD(cell_used_process, ev, data)
{ 
  static struct etimer timer;

  
  PROCESS_BEGIN();
  
  etimer_set(&timer, CLOCK_SECOND/100);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    ts = TSCH_ASN_MOD(&tsch_current_asn, sf->size);
    if(sf != NULL && tsch_current_asn.ls4b % sf->size.val == 0) {
      sf_count++;
      
      struct tsch_link *l = list_head(sf->links_list);

      while(l != NULL) {        
        if(l->cell_used_count) {     
          sf_cell_used++;
          l->cell_used_count = 0;          
        }
        l = list_item_next(l);
      }
      
      LOG_INFO("### sf_count[%u] sf_cell_usage[[%u]] node_addr %d%d >>\n", sf_count, sf_cell_used, linkaddr_node_addr.u8[0],
                                                                                                    linkaddr_node_addr.u8[1]);
      LOG_INFO_("\n");
      
      if(sf_cell_used > max_sf_cell_used) {
        max_sf_cell_used = sf_cell_used;
        LOG_INFO("MAX sf cell usage: %u", max_sf_cell_used);
        LOG_INFO_("\n");
      }    
                                                                                                
      sf_cell_used = 0;
      
    }
    etimer_reset(&timer);
  }
  PROCESS_END();
}
*/
/*----------------------------------------------------------------------*/
void
print_sf_cell_usage(uint16_t handle)
{
  uint16_t i;
  for(i=0; i<3; i++) {
    sf = tsch_schedule_get_slotframe_by_handle(i);
    if(sf != NULL) {
      sf_count++;
      
      struct tsch_link *l = list_head(sf->links_list);

      while(l != NULL) {        
        if(l->cell_used_count) {     
          sf_cell_used++;
          l->cell_used_count = 0;          
        }
        l = list_item_next(l);
      }
/*      
      LOG_INFO("### sf_count[0x%lx] sf_cell_usage[[%u]] node_addr %d%d >>\n", tsch_current_asn.ls4b, sf_cell_used, linkaddr_node_addr.u8[0],
                                                                                                    linkaddr_node_addr.u8[1]);
*/      
      if(sf_cell_used > max_sf_cell_used) {
        max_sf_cell_used = sf_cell_used;
      }    
                                                                                                
      sf_cell_used = 0;
      
    }
  } 
}
/*----------------------------------------------------------------------*/
void
print_max_sf_cell_usage(void)
{
  LOG_INFO("[MAX sf cell usage: %u node_addr %d%d ] \n", max_sf_cell_used, linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
}
/*----------------------------------------------------------------------*/

