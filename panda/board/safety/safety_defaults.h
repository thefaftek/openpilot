bool HKG_forwarding_enabled = 1;
int HKG_MDPS12_checksum = -1;
int HKG_MDPS12_cnt = 0;   
int HKG_last_StrColT = 0;

void default_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int addr = GET_ADDR(to_push);
  
  if (addr == 593) {
    if (HKG_MDPS12_checksum == -1) {
      int New_Chksum2 = 0;
      uint8_t dat[8];
      for (int i=0; i<8; i++) {
        dat[i] = GET_BYTE(to_push, i);
      }
      int Chksum2 = dat[3];
      dat[3] = 0;
      for (int i=0; i<8; i++) {
        New_Chksum2 += dat[i];
      }
      New_Chksum2 %= 256;
      if (Chksum2 == New_Chksum2) {
        HKG_MDPS12_checksum = 0;
      }
      else {
        HKG_MDPS12_checksum = 1;
      }
    }
  }
}

// *** no output safety mode ***

static void nooutput_init(int16_t param) {
  UNUSED(param);
  controls_allowed = 0;
}

static int nooutput_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);
  if ((addr == 832) && (bus == 1)) {
		uint8_t dat[8];
		int new_checksum = 0;
		for (int i=0; i<8; i++) {
		  dat[i] = GET_BYTE(to_send, i);
		}
		dat[0] &= ~0x03;
		dat[0] |= 0x02;
		dat[3] &= ~0xE0;
		dat[4] &= ~0x0C;
		dat[4] |= 0x04;
		dat[6] = new_checksum;
		dat[7] &= ~0x3F;
		dat[7] |= 0x10;
		for (int i=0; i<6; i++) {
		  new_checksum += dat[i];
		}
		dat[6] = new_checksum % 256;

		to_send->RDLR = dat[0] | dat[1] << 8 | dat[2] << 16 | dat[3] << 24;
		to_send->RDHR = dat[4] | dat[5] << 8 | dat[6] << 16 | dat[7] << 24;
  }
  return 1;
}

static int nooutput_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  UNUSED(lin_num);
  UNUSED(data);
  UNUSED(len);
  return false;
}

 static int default_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  if ((bus_num == 0) && (addr == 832)) {
    HKG_forwarding_enabled = 0;
  }

  if (HKG_forwarding_enabled) {
    if (bus_num == 0) {
      if (addr == 593) {
        uint8_t dat[8];
        int New_Chksum2 = 0;
        for (int i=0; i<8; i++) {
          dat[i] = GET_BYTE(to_fwd, i);
        }
        if (HKG_MDPS12_cnt > 330) {
          int StrColTq = dat[0] | (dat[1] & 0x7) << 8;
          int OutTq = dat[6] >> 4 | dat[7] << 4;
          if (HKG_MDPS12_cnt == 331) {
            StrColTq -= 164;
          }
          else {
            StrColTq = HKG_last_StrColT + 34;
          }
          OutTq = 2058;

          dat[0] = StrColTq & 0xFF;
          dat[1] &= 0xF8;
          dat[1] |= StrColTq >> 8;
          dat[6] &= 0xF;
          dat[6] |= (OutTq & 0xF) << 4;
          dat[7] = OutTq >> 4;
            

          to_fwd->RDLR &= 0xFFF800;
          to_fwd->RDLR |= StrColTq;
          to_fwd->RDHR &= 0xFFFFF;
          to_fwd->RDHR |= OutTq << 20;
          HKG_last_StrColT = StrColTq;

          dat[3] = 0;
          if (HKG_MDPS12_checksum == 0) { 
            for (int i=0; i<8; i++) {
              New_Chksum2 += dat[i];
            }
            New_Chksum2 %= 256;
          }
          else if (HKG_MDPS12_checksum == 1) { //we need CRC8 checksum
            uint8_t crc = 0xFF;
            uint8_t poly = 0x1D;
            int i, j;
            for (i=0; i<8; i++){
              if (i!=3){ //don't include CRC byte
                crc ^= dat[i];
                for (j=0; j<8; j++) {
                  if ((crc & 0x80) != 0U) {
                    crc = (crc << 1) ^ poly;
                  } else
                  {
                  crc <<= 1;
                  }
                }
              }
            }
            crc ^= 0xFF;
            crc %= 256;
            New_Chksum2 = crc;
          }
          to_fwd->RDLR |= New_Chksum2 << 24;
        }
        HKG_MDPS12_cnt += 1;
        HKG_MDPS12_cnt %= 345;
      }
      bus_fwd = 12;
    }
    if (bus_num == 1) {
      bus_fwd = 20;
    }
    if (bus_num == 2) {
      bus_fwd = 10;
    }
  }
  else {
    if (bus_num == 0) {
      bus_fwd = 1;
    }
    if (bus_num == 1) {
      bus_fwd = 0;
    }
  }
  return bus_fwd;
}

const safety_hooks nooutput_hooks = {
  .init = nooutput_init,
  .rx = default_rx_hook,
  .tx = nooutput_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = default_fwd_hook,
};

// *** all output safety mode ***

static void alloutput_init(int16_t param) {
  UNUSED(param);
  controls_allowed = 1;
}

static int alloutput_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);
  if ((addr == 832) && (bus == 1)) {
		uint8_t dat[8];
		int new_checksum = 0;
		for (int i=0; i<8; i++) {
		  dat[i] = GET_BYTE(to_send, i);
		}
		dat[0] &= ~0x03;
		dat[0] |= 0x02;
		dat[3] &= ~0xE0;
		dat[4] &= ~0x0C;
		dat[4] |= 0x04;
		dat[6] = new_checksum;
		dat[7] &= ~0x3F;
		dat[7] |= 0x10;
		for (int i=0; i<6; i++) {
		  new_checksum += dat[i];
		}
		dat[6] = new_checksum % 256;

		to_send->RDLR = dat[0] | dat[1] << 8 | dat[2] << 16 | dat[3] << 24;
		to_send->RDHR = dat[4] | dat[5] << 8 | dat[6] << 16 | dat[7] << 24;
  }
  return 1;
}

static int alloutput_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  UNUSED(lin_num);
  UNUSED(data);
  UNUSED(len);
  return true;
}

const safety_hooks alloutput_hooks = {
  .init = alloutput_init,
  .rx = default_rx_hook,
  .tx = alloutput_tx_hook,
  .tx_lin = alloutput_tx_lin_hook,
  .fwd = default_fwd_hook,
};
