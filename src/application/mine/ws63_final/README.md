# WS63 Final Layered Framework

## 1. ¼Ü¹¹¶¨Î»

±¾ÏµÍ³ÃæÏòÃÅËøÖÕ¶ËµÄ¶à´«¸ĞÆ÷Ğ­Í¬Óë¶àÁ´Â·Í¨ĞÅĞèÇó£¬½áºÏ `ws63_final` µÄÓ¦ÓÃ·Ö²ãÓë `sle_uart_host` µÄÖ÷»úÇÅ½Ó·½Ê½£¬²ÉÓÃ×Ôµ×ÏòÉÏµÄ·Ö²ãÊ½Èí¼ş¼Ü¹¹½øĞĞÉè¼Æ¡£ÕûÌå½á¹¹ÓÉ `BSP`¡¢`Driver`¡¢`Middleware`¡¢`App/Task` Óë `App/Main` Îå¸ö²ã¼¶×é³É£¬²¢Í¨¹ıÖ÷»ú²à `sle_uart_host` Íê³É UART0 Óë SLE/SSAPS Ö®¼äµÄÃüÁî½ÓÈë¡¢Êı¾İ×ª·¢ºÍ¿ÉÊÓ»¯µ÷ÊÔ¡£¸Ã¼Ü¹¹µÄ»ù±¾Ô­ÔòÊÇµ¥ÏòÒÀÀµ£ºÉÏ²ã½öÄÜµ÷ÓÃÏÂ²ãÌá¹©µÄ½Ó¿Ú£¬ÏÂ²ã²»µÃ·´ÏòÒÀÀµÉÏ²ãÒµÎñÂß¼­¡£ÕâÑù¿ÉÒÔ°ÑÓ²¼ş¼Ä´æÆ÷·ÃÎÊ¡¢Ğ­ÒéÏ¸½Ú´¦ÀíºÍÒµÎñ±àÅÅÑÏ¸ñ·ÖÀë£¬±ÜÃâÒµÎñ´úÂëÖ±½ÓñîºÏµ×²ãÊµÏÖ¡£

´ÓÂÛÎÄ±í´ï½Ç¶È¿´£¬ÕâÒ»¼Ü¹¹µÄºËĞÄ¼ÛÖµÔÚÓÚ½«¡°ÈÎÎñµ÷¶È¡¢Ğ­Òé×ª»»¡¢Ó²¼ş·ÃÎÊ¡¢ÒµÎñ±àÅÅ¡±ËÄÀàÖ°Ôğ²ğ·Öµ½²»Í¬Ä£¿éÖĞ£¬Ê¹Ã¿Ò»²ã¶¼Ö»¹Ø×¢×Ô¼ºËùÔÚ±ß½çÄÚµÄÎÊÌâ¡£¶ÔÓÚÊµÊ±ĞÔÒªÇó½Ï¸ß¡¢ÍâÉèÀàĞÍ½Ï¶àµÄÇ¶ÈëÊ½ÃÅËøÏµÍ³£¬ÕâÖÖ·½Ê½¿ÉÒÔÏÔÖø½µµÍÄ£¿éñîºÏ¶È£¬ÌáÉı¿ÉÎ¬»¤ĞÔ¡¢¿ÉÒÆÖ²ĞÔºÍºóĞøÀ©Õ¹ÄÜÁ¦£¬Í¬Ê±Ò²±ãÓÚÔÚĞÂÔö´«¸ĞÆ÷»òĞÂÔöÍ¨ĞÅÁ´Â·Ê±±£³Ö½Ó¿ÚÎÈ¶¨¡£

### 1.1 Éè¼ÆÄ¿±ê

1. ÃæÏò¶à´«¸ĞÆ÷Ğ­Í¬³¡¾°£¬Í³Ò»´¦Àí camera¡¢LD2402¡¢ZW101¡¢TTP229¡¢RGB¡¢·äÃùÆ÷¡¢µç»úÓë±àÂëÆ÷µÈÍâÉèÊı¾İ¡£
2. ÃæÏò¶àÁ´Â·Í¨ĞÅ³¡¾°£¬½«±¾µØ UART ¿ØÖÆ¡¢WK2114 ×Ó´®¿ÚÀ©Õ¹Óë SLE/SSAPS ÎŞÏßÇÅ½ÓÍ³Ò»ÄÉÈëÍ¬Ò»Ì×µ÷¶È¿ò¼Ü¡£
3. ÃæÏòÃÅËøºËĞÄÒµÎñ£¬½«×´Ì¬¹ÜÀí¡¢ÈÏÖ¤ÅĞ¶¨¡¢¿ªËø¿ØÖÆÓë¸æ¾¯´¦ÀíÊÕÁ²µ½¶ÀÁ¢µÄÒµÎñ±àÅÅ²ã£¬±ÜÃâÒµÎñÂß¼­É¢ÂäÔÚÇı¶¯Ï¸½ÚÖĞ¡£
4. ÃæÏò¹¤³Ì¿ÉÎ¬»¤ĞÔ£¬È·±£ÈÎºÎĞÂÔö¹¦ÄÜ¶¼ÄÜÃ÷È·ÂäÎ»µ½¶ÔÓ¦²ã¼¶£¬¶ø²»ÊÇ¿ç²ãĞŞ¸Ä¶à¸öÄ£¿é¡£

### 1.2 ·Ö²ãÖ°Ôğ

1. `App/Main`£ºÏµÍ³Ö÷Èë¿Ú£¬Ö»¸ºÔğ´´½¨ÒµÎñ×ÜÈÎÎñ£¬²»³ĞÔØ¾ßÌåÒµÎñÂß¼­¡£
2. `App/Task`£ºÓ¦ÓÃÈÎÎñ²ã£¬¸ºÔğÈÎÎñ³õÊ¼»¯¡¢ÂÖÑ¯µ÷¶È¡¢ÏûÏ¢¶ÓÁĞ·Ö·¢¡¢ÈÏÖ¤×´Ì¬»ú¡¢´«¸ĞÆ÷½á¹ûÈÚºÏÒÔ¼°ÃÅËøÒµÎñ±àÅÅ¡£
3. `Middleware`£ºÖĞ¼ä¼ş²ã£¬¸ºÔğ OSAL ·â×°¡¢SLE Á¬½ÓÓë SSAPS ×ª·¢µÈÆ½Ì¨ÄÜÁ¦£¬°ÑĞ­ÒéÁ´Â·ºÍÏµÍ³³éÏó·â×°ÎªÎÈ¶¨½Ó¿Ú¡£
4. `Driver`£ºÉè±¸Çı¶¯²ã£¬¸ºÔğ WK2114¡¢¶àÂ·×Ó´®¿Ú¡¢LD2402¡¢ZW101¡¢camera µÈÍâÉèĞ­ÒéÊµÏÖ£¬ÏòÉÏÖ»Êä³ö±ê×¼»¯Êı¾İºÍ¿ØÖÆ½á¹û¡£
5. `BSP`£º°å¼¶Ö§³Å²ã£¬¸ºÔğ GPIO¡¢UART¡¢I2C¡¢SPI¡¢PWM¡¢IRQ µÈµ×²ã×ÊÔ´·ÃÎÊ£¬ÊÇÎ¨Ò»ÔÊĞíÖ±½Ó½Ó´¥ WS63 Ó²¼ş³éÏó½Ó¿ÚµÄ²ã¼¶¡£

Ö÷»ú²à `sle_uart_host` Î»ÓÚÏµÍ³Íâ²¿£¬³Ğµ£¡°ÉÏÎ»»úÃüÁîÈë¿Ú + SLE/SSAPS ÇÅ½ÓÆ÷¡±µÄ½ÇÉ«¡£ËüÍ¨¹ı UART0 ½ÓÊÕµ÷ÊÔÖ¸Áî£¬ÔÙ½«ÃüÁî×ª·¢µ½ SLE ÏÂĞĞÁ´Â·£»Í¬Ê±½ÓÊÕ ws63_final µÄ SLE ÉÏĞĞÊı¾İ£¬²¢°Ñ½á¹û»ØÏÔµ½Ö÷»ú´®¿Ú¡£ÕâÑù¿ÉÒÔ°Ñ±¾µØ¿ØÖÆÌ¨ºÍÎŞÏßÁ´Â·Í³Ò»ÎªÍ¬Ò»Ìõµ÷ÊÔ±Õ»·¡£

### 1.3 Æô¶¯Á´Â·

ÏµÍ³Æô¶¯ºó£¬`App/Main` ÏÈ´´½¨ `App/Task` ÈÎÎñÈë¿Ú£¬ÔÙÓÉÈÎÎñ²ã°´³õÊ¼»¯Ë³ĞòÍê³É `Middleware`¡¢`Driver` Óë `BSP` µÄÄÜÁ¦×°Åä¡£Ëæºó£¬¸÷ÒµÎñ×ÓÄ£¿é½øÈë¶ÀÁ¢ÈÎÎñ»ò×´Ì¬»úÔËĞĞ½×¶Î£¬ÀıÈçµç»úÓë±àÂëÆ÷ÓÃÓÚÔË¶¯¿ØÖÆ£¬LD2402 ÓÃÓÚ¾àÀë¼ì²â£¬ZW101 ÓÃÓÚÖ¸ÎÆÈÏÖ¤£¬TTP229 ÓÃÓÚÃÜÂëÊäÈë£¬camera ÓÃÓÚÊÓ¾õÊ¶±ğ£¬RGB Óë·äÃùÆ÷ÓÃÓÚ×´Ì¬·´À¡¡£Ö÷ÈÎÎñ¸ºÔğ°ÑÕâĞ©Ä£¿éµÄ½á¹û»ã×Ü³ÉÃÅËø×´Ì¬£¬²¢ÔÚ±ØÒªÊ±´¥·¢¿ªËø¡¢±£³Ö¡¢¸æ¾¯»ò»Ö¸´¶¯×÷¡£

### 1.4 Êı¾İÁ÷×éÖ¯

ÏµÍ³ÔËĞĞÊ±£¬Êı¾İÁ´Â·Í¨³£×ñÑ­¡°²É¼¯ - ·Ö·¢ - ´¦Àí - »Ø´«¡±µÄË³Ğò×éÖ¯£º

1. ÍâÉèÔÚ `Driver` »ò `BSP` ²ãÍê³É²ÉÑùÓëĞ­ÒéÊÕ°ü¡£
2. ²É¼¯½á¹û½øÈë `App/Task` ²ãµÄ·Ö·¢¶ÓÁĞ»ò»Øµ÷Èë¿Ú¡£
3. ÒµÎñÈÎÎñ¸ù¾İ´«¸ĞÆ÷½á¹ûÍê³ÉÈÏÖ¤ÅĞ¶Ï¡¢×´Ì¬¸üĞÂºÍ¶¯×÷¾ö²ß¡£
4. ¾ö²ß½á¹ûÔÙÍ¨¹ı `Middleware` »òÖ÷»ú²à `sle_uart_host` »Ø´«µ½ÉÏÎ»»ú£¬ĞÎ³É±Õ»·¹Û²âÁ´Â·¡£

ÕâÖÖ×éÖ¯·½Ê½µÄ¹Ø¼üÔÚÓÚ½âñî¡£ÈÎÎñÖ®¼äÍ¨¹ı¶ÓÁĞ¡¢»Øµ÷ºÍ×´Ì¬±êÖ¾½øĞĞ½»»¥£¬±ÜÃâÍ¬²½×èÈûºÍ¿ç²ãÖ±½Óµ÷ÓÃ£»Ö÷»úÓëÉè±¸Ö®¼äÍ¨¹ı SLE/SSAPS ºÍ UART ÇÅ½Ó½øĞĞÍ¨ĞÅ£¬±ÜÃâÒµÎñÂß¼­Ö±½ÓÒÀÀµ¾ßÌå´®¿ÚÊµÏÖ¡£

### 1.5 ¼Ü¹¹ÊÕÒæ

1. ¿ÉÎ¬»¤ĞÔ¸üÇ¿£ºÃ¿¸öÄ£¿éÖ°Ôğµ¥Ò»£¬ÎÊÌâ¶¨Î»Ê±Ö»Ğè¹Ø×¢¶ÔÓ¦²ã¼¶¡£
2. ¿ÉÒÆÖ²ĞÔ¸üºÃ£ºµ×²ãÓ²¼ş²îÒìÖ÷Òª¼¯ÖĞÔÚ `BSP` Óë `Driver`£¬ÒµÎñ²ãÎŞĞè¸ĞÖª¾ßÌå¼Ä´æÆ÷ºÍÒı½Å±ä»¯¡£
3. ¿ÉÀ©Õ¹ĞÔ¸ü¸ß£ºĞÂÔöÍâÉèÊ±£¬Ö»ĞèÔÚ `Driver` »ò `App/Task` Ôö¼Ó¶ÔÓ¦Ä£¿é£¬²¢Í¨¹ıÍ³Ò»½Ó¿Ú½ÓÈëÖ÷µ÷¶È¡£
4. ÊµÊ±ĞÔ¸üÎÈ¶¨£ºÈÎÎñµ÷¶È¡¢Ğ­ÒéÊÕ·¢ºÍÓ²¼ş·ÃÎÊ·Ö²ãºó£¬¹Ø¼üÂ·¾¶¸üÇåÎú£¬±ãÓÚ¿ØÖÆ×èÈûµãºÍ²¢·¢³åÍ»¡£
5. ¹¤³Ì±ß½ç¸üÃ÷È·£ºÃÅËøÒµÎñÂß¼­¡¢Ğ­Òé½âÎöÓëÓ²¼ş¿ØÖÆ±Ë´Ë¸ôÀë£¬½µµÍºóĞøÖØ¹¹µÄ³É±¾¡£

## 2. ÏµÍ³¼Ü¹¹Í¼

```mermaid
flowchart TB
	subgraph HOST[Ö÷»ú²à sle_uart_host]
		HOST_UART[UART0 ÃüÁîÈë¿Ú]
		HOST_SLE[SLE / SSAPS Á¬½ÓÓë×ª·¢]
		HOST_APP[Ö÷»ú²àµ÷ÊÔÓë¿ØÖÆÂß¼­]
		HOST_UART --> HOST_APP --> HOST_SLE
	end

	subgraph WS63[ws63_final]
		MAIN[App/Main\nÈë¿ÚÆô¶¯]
		TASK[App/Task\nÈÎÎñµ÷¶È / ÒµÎñ±àÅÅ]
		MIDDLE[Middleware\nOSAL / SLE ÖĞ¼ä¼ş]
		DRIVER[Driver\nÍâÉèĞ­ÒéÇı¶¯]
		BSP[BSP\nGPIO / UART / I2C / SPI / PWM / IRQ]
		MAIN --> TASK --> MIDDLE --> DRIVER --> BSP
	end

	HOST_SLE <-->|SLE / SSAPS\nÃüÁîÓëÊı¾İË«Ïò×ª·¢| MIDDLE
	TASK -->|´«¸ĞÆ÷½á¹û / ¿ØÖÆÖ¸Áî| DRIVER
	DRIVER -->|Ó²¼ş·ÃÎÊ| BSP
```

## 3. Ä¿Â¼½á¹¹

```text
ws63_final/
©À©¤©¤ Config/      # ²ÎÊıÅäÖÃ²ã£¨²¨ÌØÂÊ¡¢Òı½Å¡¢ÈÎÎñ²ÎÊı£©
©À©¤©¤ Common/      # ¹«¹²Ëã·¨/Í¨ÓÃ¹¤¾ß£¨ÎŞÓ²¼şÒÀÀµ£©
©À©¤©¤ BSP/         # WS63 Ó²¼ş³éÏó²ã£¨GPIO/UART/IRQ£©
©À©¤©¤ Driver/      # WK2114 Éè±¸Çı¶¯²ã£¨¼Ä´æÆ÷/FIFO/×Ó´®¿Ú·â×°£©
©À©¤©¤ Middleware/  # OSAL ³éÏó£¨ÈÎÎñ¡¢ÑÓÊ±¡¢tick£©
©À©¤©¤ App/Task/    # Ó¦ÓÃÈÎÎñ²ã£¨ÂÖÑ¯µ÷¶È¡¢»Øµ÷·Ö·¢£©
©¸©¤©¤ App/Main/    # Ó¦ÓÃÖ÷Èë¿Ú£¨ÈÎÎñÆô¶¯£©
```

## 4. ¶ÔÍâ½Ó¿Ú

1. `ws63_start()`£ºÆô¶¯×îÖÕ°æÒµÎñÈÎÎñ¡£
2. `ws63_task_register_rx_callback(sub_port, cb)`£º×¢²á×Ó´®¿Ú½ÓÊÕ»Øµ÷¡£
3. `ws63_task_send(sub_port, data, len)`£ºÍ¨¹ıÖ¸¶¨×Ó´®¿Ú·¢ËÍÊı¾İ¡£
4. `ws63_task_buzzer_on(freq_hz)` / `ws63_task_buzzer_off()`£º¿ØÖÆ·äÃùÆ÷¿ª¹ØÓëÆµÂÊ¡£
5. `ws63_task_buzzer_set_volume(volume_percent)`£ºÉèÖÃ·äÃùÆ÷ÒôÁ¿£¨Õ¼¿Õ±ÈÓ³Éä£©¡£
6. `ws63_task_ld2402_reinit()` / `ws63_task_zw101_reinit()`£º´¥·¢Ä£¿éµ÷ÊÔÎÕÊÖÖØ³õÊ¼»¯¡£
7. `ws63_task_zw101_echo()/verify()/enroll()/list()/delete()/clear()/cancel()`£ºZW101 ÖØ¹¹ºóµÄ×îĞ¡ÒµÎñ½Ó¿Ú¼¯ºÏ¡£
8. `ws63_zw101_task_start()` / `ws63_task_zw101_request_verify()`£ºÃÅËø½Ó½ü´°¿ÚÄÚ´¥·¢ VERIFY ²¢µÈ´ıÈÏÖ¤½á¹û¡£

## 5. ºóĞøÄ£¿éÕûºÏ½¨Òé

1. Ã¿¸öÒµÎñÄ£¿é£¨Èç LD2402/ZW101£©ÔÚ `App/Task` ²ã×¢²á×Ô¼ºµÄ `rx_callback`¡£
2. ÒµÎñÄ£¿é½ûÖ¹Ö±½Ó·ÃÎÊ BSP/¼Ä´æÆ÷£¬Ö»ÄÜµ÷ÓÃ Task/Driver ±©Â¶µÄ±ê×¼½Ó¿Ú¡£
3. Èç¹ûĞÂÔöÓ²¼ş²îÒì£¨Òı½Å¡¢²¨ÌØÂÊ¡¢FIFO ãĞÖµ£©£¬ÓÅÏÈ¸Ä `Config`£¬²»¸ÄÒµÎñÂß¼­¡£

## 6. ±àÒë¿ª¹Ø

ÔÚ menuconfig ÖĞ¿ªÆô£º

- `Application -> Mine -> Support Mine WS63 final layered framework (WS63 master).`

## 7. ±¸×¢

µ±Ç°¿ò¼ÜÒÑÍê³É»ù´¡Á´Â·£º

1. WS63 Ö÷¿Ú³õÊ¼»¯¡£
2. WK2114 Ö÷¿Ú×Ô¶¯²¨ÌØÆ¥Åä¡£
3. ×Ó´®¿Ú°´ÅäÖÃ³õÊ¼»¯¡£
4. ÂÖÑ¯¶ÁÈ¡²¢»Øµ÷·Ö·¢¡£

ÄãºóĞøÖ»ĞèÒªÔÚ `App/Task` ²ãÀ©Õ¹ÒµÎñÂß¼­£¬¼´¿ÉÍê³É¶àÄ£¿éÍ³Ò»ÕûºÏ¡£

## 8. µ÷ÊÔÃüÁîÎÄµµ

- ´®¿ÚÔÚÏß¿Ø²âÃüÁîÓëÈÕÖ¾ËµÃ÷Çë²é¿´£º`DEBUG_COMMANDS.md`

## 9. ÈÎÎñÎ¬»¤¼ÇÂ¼

### 2026-04-14: README ¼Ü¹¹ËµÃ÷°´ ws63_final Óë sle_uart_host ¿Ú¾¶Í³Ò»

±ä¸üÕªÒª£º
- ½«¼Ü¹¹¶¨Î»¸ÄĞ´ÎªÃæÏòÃÅËøÖÕ¶Ë¶à´«¸ĞÆ÷Óë¶àÁ´Â·Í¨ĞÅµÄ·Ö²ãÊ½ËµÃ÷£¬Ã÷È· ws63_final Óë sle_uart_host µÄÖ°Ôğ±ß½ç¡£
- ²¹³äÏµÍ³¼Ü¹¹ Mermaid Í¼£¬Õ¹Ê¾Ö÷»ú²à UART0 -> SLE/SSAPS -> ws63_final µÄÍêÕûÊı¾İÁ´Â·¡£
- ±£ÁôÔ­ÓĞÄ¿Â¼½á¹¹ÓëºóĞøÄ£¿éÕûºÏ½¨Òé£¬²¢°Ñ²ã¼¶ÃèÊöÊÕÁ²µ½ App/Main¡¢App/Task¡¢Middleware¡¢Driver¡¢BSP µÄµ¥ÏòÒÀÀµ¹ØÏµ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÎÄµµ¸üĞÂ£¬ÎŞĞè±àÒë¡£

·çÏÕ/ºóĞø£º
- ÈôºóĞø ws63_final ĞÂÔö¶ÀÁ¢·şÎñ²ã»òĞÂµÄÖ÷»úÇÅ½ÓÁ´Â·£¬ÔÙÍ¬²½µ÷Õû Mermaid Í¼ÖĞµÄÄ£¿é±ß½ç¡£

### 2026-04-14: README ¼Ü¹¹¶ÎÀ©Õ¹ÎªÂÛÎÄÕıÎÄ

±ä¸üÕªÒª£º
- ½«¼Ü¹¹¶¨Î»À©Õ¹ÎªÊÊºÏÂÛÎÄÖ±½ÓÒıÓÃµÄÕıÎÄ±í´ï£¬²¹³äÉè¼ÆÄ¿±ê¡¢·Ö²ãÖ°Ôğ¡¢Æô¶¯Á´Â·¡¢Êı¾İÁ÷×éÖ¯ºÍ¼Ü¹¹ÊÕÒæ¡£
- ±£ÁôÔ­ÓĞ Mermaid Í¼ºÍÄ¿Â¼½á¹¹£¬È·±£ÂÛÎÄÕıÎÄÓë¹¤³ÌËµÃ÷±£³ÖÒ»ÖÂ¡£
- Ç¿»¯Ö÷»ú²à `sle_uart_host` Óë ws63_final Ö®¼äµÄÇÅ½Ó¹ØÏµÃèÊö£¬±ãÓÚ´ÓÏµÍ³¼¶ËµÃ÷ÕûÌåÈí¼ş±Õ»·¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÎÄµµ¸üĞÂ£¬ÎŞĞè±àÒë¡£

·çÏÕ/ºóĞø£º
- ÈôÂÛÎÄºóĞø»¹ĞèÒª¡°Ê±ĞòÍ¼¡±»ò¡°Ä£¿éĞ­×÷Í¼¡±£¬¿É¼ÌĞøÔÚ±¾ÎÄ¼ş²¹³ä¶ÔÓ¦ Mermaid Í¼¡£

### 2026-04-14: ¹ÜÀíÈÎÎñÕ»ÌáÉıµ½ 8KB

±ä¸üÕªÒª£º
- ½« `WS63_MGR_TASK_STACK_SIZE` ÌáÉıµ½ `8192U`£¬±ÜÃâµ÷ÊÔÃüÁî¡¢×´Ì¬»úºÍÈÕÖ¾ÔÚÍ¬Ò»¹ÜÀíÈÎÎñÄÚµş¼ÓÊ±·¢ÉúÕ»Òç³ö¡£
- ±£Áô `WS63_TASK_STACK_SIZE` ×÷Îª·Ç¹ÜÀí³¡¾°Ä¬ÈÏÖµ£¬²»Ó°ÏìÆäËüÈÎÎñµÄÕ»ÅäÖÃ¡£
- Í¬²½¸üĞÂÅäÖÃ×¢ÊÍ£¬Ã÷È· 8KB Ô¤Áô¸øÃüÁî½âÎöºÍÈÕÖ¾·åÖµ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` ºÍ `packet success!`¡£

·çÏÕ/ºóĞø£º
- ÈôºóĞø¹ÜÀíÈÎÎñ¼ÌĞøµş¼Ó¸ü¶àµ÷ÊÔÈë¿Ú£¬ÔÙ½áºÏÕ»Ë®Î»Í³¼ÆÆÀ¹ÀÊÇ·ñĞèÒª½øÒ»²½Ôö´ó¡£

### 2026-04-14: LD2402 LOG ON/OFF »Ö¸´Ö÷»úÉÏĞĞ¿ØÖÆ

±ä¸üÕªÒª£º
- ½« `LD2402` Ö÷»ú²àÉÏĞĞÖØĞÂ°ó¶¨µ½ÈÕÖ¾¿ª¹Ø£¬`LD LOG OFF` ¸ºÔğ¾²Òô£¬`LD LOG ON` ¸ºÔğ»Ö¸´¾àÀëĞĞ¡£
- ±£³Ö `ZW101` µÄ Hex Ô¤ÀÀ²ßÂÔ²»±ä£¬±ÜÃâÔ­Ê¼¶ş½øÖÆ¼ÌĞøË¢µ½Ö÷»úÖÕ¶Ë¡£
- ÕâÑù `LD LOG ON` / `OFF` µÄÏÖ³¡·´À¡ÓëÃüÁîÓïÒåÖØĞÂÒ»ÖÂ£¬±ãÓÚÅÅÕÏÊ±ÁÙÊ±¿ª¹Ø¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ´ıÖØĞÂ¹¹½¨ÑéÖ¤¡£

·çÏÕ/ºóĞø£º
- `LD LOG ON` ÏÖÔÚ»á»Ö¸´Ö÷»ú²à¾àÀëĞĞ£¬ÈôĞèÒªÔÙ´Î¾²Òô£¬¼ÌĞøÖ´ĞĞ `LD LOG OFF` ¼´¿É¡£

### 2026-04-14: DEBUG INIT Áª¶¯¹Ø LD ÈÕÖ¾²¢È¡Ïû ZW101

±ä¸üÕªÒª£º
Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ´ıÖ´ĞĞ¹¹½¨ÑéÖ¤¡£

### 2026-04-14: LD2402 Ö÷»ú¾²Òô + ZW101 Hex Ô¤ÀÀ

±ä¸üÕªÒª£º
- ÔÚ `ws63_task_post_sle_uplink()` Àï½« `LD2402` ×Ó¿ÚÉÏĞĞÖ±½Ó¾²Òô£¬²»ÔÙ°Ñ¾àÀëË¢ÆÁÍ¸´«µ½Ö÷»ú¡£
- ½« `ZW101` ×Ó¿ÚÔ­Ê¼¶ş½øÖÆÉÏĞĞÊÕÁ²Îª¶Ì Hex Ô¤ÀÀÎÄ±¾£¬±ÜÃâÖ÷»úÖÕ¶ËÖ±½ÓÏÔÊ¾ÂÒÂë¡£
- ĞÂÔö±¾µØ Hex Ô¤ÀÀÖúÊÖ£¬±£ÁôÖ¡Í·ºÍÇ°¼¸¸ö×Ö½Ú£¬±ãÓÚÈ·ÈÏÁ´Â·ÈÔÈ»¿É¶Á¡£
- ÈÔ±£Áô `DEBUG INIT` Ö®ºóµÄµ÷ÊÔÃüÁî»ØÖ´Óë±ØÒª×´Ì¬ÈÕÖ¾£¬²»Ó°ÏìÃüÁî½»»¥¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬´ò°üÁ÷³ÌÒÑÍê³É£¬ÈÕÖ¾Î²²¿°üº¬ `packet app` Óë `copy_files_to_interim done!`¡£

·çÏÕ/ºóĞø£º
- `ZW101` Ä¿Ç°Ö»Õ¹Ê¾ Hex Ô¤ÀÀ£¬²»ÔÙÍ¸´«ÍêÕûÔ­Ê¼Ö¡£»ÈôºóĞøĞèÒª×¥ÍêÕû°ü£¬ĞèÒªµ¥¶ÀÔö¼ÓÊÜ¿Øµ÷ÊÔÈë¿Ú¡£

### 2026-04-14: DEBUG INIT ´¿µ÷ÊÔ»á»°ÃÅ¿Ø

±ä¸üÕªÒª£º
- ĞÂÔö `DEBUG INIT` / `DEBUG EXIT` / `DEBUG STAT` Èı¸ö»á»°ÃüÁî£¬ÓÃÓÚÔÚÔËĞĞÊ±ÇĞ»»´¿µ÷ÊÔÄ£Ê½¡£
- µ÷ÊÔÄ£Ê½¿ªÆôºó£¬ÃÅËø±àÅÅÈÎÎñ¸ÄÎªÑÓºóÆô¶¯£»ÈôÒÑÆô¶¯£¬Ôò½øÈëĞü¹ÒÌ¬²¢Çå¿ÕÀúÊ·ÈÏÖ¤ÊÂ¼ş£¬±ÜÃâÍË³öµ÷ÊÔºóÎó´¥·¢¿ªËø¡£
- `ws63_task_entry` ¸ÄÎªÏÈ´¦Àíµ÷ÊÔÃüÁî¡¢ÔÙ°´¹Û²ì´°¿Ú¾ö¶¨ÊÇ·ñÀ­Æğ `lock_mgr`£¬´Ó¶ø¸øÖ÷»úÊäÈë `DEBUG INIT` Áô³öÈë¿Ú¡£
- Î´½øÈë `DEBUG INIT` Ê±£¬Éè±¸²à²»ÔÙÏòÖ÷»úÉÏ±¨ LD2402 / ZW101 µÈ³£¹æÔËĞĞÈÕÖ¾£¬Ö÷»ú´®¿ÚÖ»±£Áô±ØÒªÁ´Â·Êä³ö¡£
- Í¬²½¸üĞÂ `DEBUG_COMMANDS.md`£¬²¹³äµ÷ÊÔ»á»°ÓïÒåÓëÅÅÕÏËµÃ÷¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-14: ÑÏ¸ñ SLE Ö÷´Óµ÷ÊÔÁ´Â·ÊÕÁ²£¨Host ¿ØÖÆ + Slave »ØÏÔ£©

±ä¸üÕªÒª£º
- ĞÂÔö `ws63_final` ÑÏ¸ñÄ£Ê½¿ª¹Ø `WS63_DEBUG_STRICT_SLE_ONLY`£¬²¢ÔÚ±àÒëÆÚÇ¿ÖÆÔ¼Êø£º`SLE_CMD=1`¡¢`SLE_LOG=1`¡¢`LOCAL_UART_IO=0`¡£
- Ö÷»ú `sle_uart_host` Ä¬ÈÏÃüÁîÈë¿ÚÊÕÁ²µ½ `UART0`£¬²¢ĞÂÔöÑÏ¸ñÈë¿Ú¿ª¹Ø£¬±ÜÃâ·ÇÃüÁî´®¿ÚÔëÉùÎóÏÂ·¢µ½´Ó»ú¡£
- Ö÷»úÔö¼Ó `[DEBUG]` ±êÇ©·ÖÁ÷´òÓ¡£ºÊÕµ½´Ó»úµ÷ÊÔÉÏĞĞºóÖ±½ÓÊä³ö `[mine host][DEBUG]` ¿É¶ÁÈÕÖ¾Ô¤ÀÀ¡£
- Í¬²½¸üĞÂ `DEBUG_COMMANDS.md`£¬²¹ÆëÑÏ¸ñÄ£Ê½¿ª¹ØÓëÖ÷»ú²àÅäÖÃ/ÅÅÕÏ¿Ú¾¶¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/sle_uart_host/inc/sle_uart_host.h`
- `src/application/mine/sle_uart_host/src/sle_uart_host.c`
- `src/application/mine/sle_uart_host/src/sle_uart_host_ssaps.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: TTP229 ¸ÄÎª°´ÏÂ»º´æ¡¢Ì§ÆğÌá½»

±ä¸üÕªÒª£º
- ½« TTP229 ÃÜÂëÊäÈë¸Ä³É±ßÑØÓïÒå£º°´ÏÂÖ»¸ºÔğ·äÃùÌáÊ¾²¢ĞøÃü auth window£¬Ì§ÆğºóÔÙÌá½»Ò»´ÎÊäÈë¡£
- ĞÂÔö pending-mask »º´æ£¬±ÜÃâ³¤°´Í¬Ò»°´¼üÊ±ÖØ¸´ÈëÂë»òÖØ¸´´¥·¢ `#` Ìá½»¡£
- ±£Áôµ¥¼üÊäÈëÁ÷³Ì£¬ÇÒÔÚ¶à¼ü»òÒì³£×´Ì¬Ê±Çå¿Õ pending£¬±ÜÃâÔà×´Ì¬ÑØÓÃµ½ÏÂÒ»´ÎÊäÈë¡£
- Î¬³ÖÏÖÓĞÃÜÂëĞ£ÑéÓë±¨¾¯ÉÏ±¨Á´Â·²»±ä£¬Ö»µ÷Õû Task ²ãµÄÊäÈëÏû·ÑÊ±»ú¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-14: TTP229 ĞøÃüÈÕÖ¾¸ÄÎªÌ§Æğºó³É¹¦Ìá½»

±ä¸üÕªÒª£º
- ½« TTP229 µÄĞøÃüÈÕÖ¾´Ó¡°Ã¿¸ö°´×¡²ÉÑù¡±Ç¨ÒÆµ½¡°Ì§ÆğºóÌá½»³É¹¦¡±Â·¾¶£¬¼õÉÙ°´×¡½×¶ÎË¢ÆÁ¡£
- °´¼ü°´ÏÂ½×¶ÎÖ»±£Áô·äÃùÌáÊ¾Óë×´Ì¬»º´æ£¬auth window ĞøÃüÑÓºóµ½³É¹¦Ìá½»ºóÔÙÖ´ĞĞ¡£
- `#` µÄ×îÖÕĞ£ÑéÈÔ±£ÁôÌ§Æğ´¥·¢ÓïÒå£¬³É¹¦Ğ£Ñéºó²¹Ò»´ÎĞøÃü²¢´òÓ¡ÏÔÊ½ÈÕÖ¾¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ´ıÖ´ĞĞ¹¹½¨ÑéÖ¤¡£

### 2026-04-13: TTP229 ĞøÃüÏÔÊ½ÈÕÖ¾²¹Æë

±ä¸üÕªÒª£º
- ÔÚ TTP229 ²ÉÑùÈë¿ÚÔö¼ÓÏÔÊ½ĞøÃüÈÕÖ¾£¬½öÔÚ `armed` ÇÒË¢ĞÂ³É¹¦Ê±Êä³ö¡£
- ÈÕÖ¾ÄÚÈİ°üº¬°´¼üÎÄ±¾/Î»Í¼£¬ÒÔ¼°ĞøÃüÇ°ºóµÄ auth window deadline£¬±ãÓÚÏÖ³¡È·ÈÏ¡°ÈÎÒ»°´¼ü°´ÏÂ¶¼»áĞøÃü¡±¡£
- ±£³ÖËø¹ÜÀíÆ÷Ö»¸ºÔğ×´Ì¬ÃÅ¿Ø£¬²»°ÑÈÕÖ¾Âß¼­ÏÂ³Áµ½×´Ì¬»úÄÚ²¿¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: VERIFY Ê§°Üºó¸ÄÎª 0.3s ÂÖÑ¯ÀëÊÖ£¨½ö ACK=0x02 ²ÅÖØÊÔ£©

±ä¸üÕªÒª£º
- µ÷Õû `ws63_task_zw101_request_verify_after_release()`£ºVERIFY Ê§°ÜºóÍ³Ò»½øÈë `wait_release`£¬²»ÔÙ¶Ô `NOT_PRESSED` Á¢¼´ÖØÊÔ¡£
- ½«ÀëÊÖÂÖÑ¯¼ä¸ôµ÷ÕûÎª `300ms`£¬Ã¿´Îµ÷ÓÃ `PS_GetImageInfo(0x3D)` ¼ì²â°´Ñ¹×´Ì¬¡£
- ½öÔÚÈ·ÈÏ `ACK=0x02`£¨ÎŞÊÖÖ¸£©ºó²ÅÅÅ¶ÓÏÂÒ»´Î VERIFY£¬±ÜÃâÁ¬Ğø¿Õ¼ìµ¼ÖÂ¸ßÆµÖØÈë¡£
- ĞŞÕı ZW101 ½ûÓÃ¼ÆÊı¹æÔò£º`ACK=0x09 (NOT_PRESSED)` ²»ÔÙ¼ÆÈë `fail_streak`£¬±ÜÃâ¡°Î´°´Ñ¹¡±°Ñ½ûÓÃãĞÖµ´òÂú¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- menuconfig Ô¤¼ì²éÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114 && bash tools/check_ws63_menuconfig.sh`
- Ô¤¼ì²é½á¹û£ºÊ§°Ü£¨½Å±¾È±Ê§£¬`No such file or directory`£©¡£
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: ÀëÊÖ¼ì²âÇĞ»»Îª PS_GetImageInfo£¨ACK=0x02 ÎŞÊÖÖ¸£©

±ä¸üÕªÒª£º
- ½« ZW101 ÀëÊÖ¼ì²â½Ó¿Ú `zw101_check_finger_present()` µÄµ×²ãÃüÁî´Ó `CheckSensor(0x36)` ÇĞ»»Îª `PS_GetImageInfo(0x3D)`¡£
- Ã÷È· ACK ÓïÒå£º`0x00=ÓĞÊÖÖ¸`¡¢`0x02=ÎŞÊÖÖ¸`£¬ÓëÏÖ³¡ÅĞ¶¨¿Ú¾¶±£³ÖÒ»ÖÂ¡£
- ±£³ÖÂÖÑ¯Â·¾¶ `trace_silent=1U`£¬±ÜÃâ¸ßÆµÀëÊÖ¼ì²âµ¼ÖÂ´®¿ÚÈÕÖ¾Ë¢ÆÁ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/zw101.h`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- menuconfig Ô¤¼ì²éÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114 && bash tools/check_ws63_menuconfig.sh`
- Ô¤¼ì²é½á¹û£ºÊ§°Ü£¨½Å±¾È±Ê§£¬`No such file or directory`£©¡£
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: ZW101 NOT_PRESSED Ê§°Ü¼ÆÊı¸ôÀë£¨±ÜÃâ¿ìËÙ lockout/½ûÓÃ£©

±ä¸üÕªÒª£º
- ÔÚ ZW101 task ĞÂÔö `ws63_task_zw101_get_last_verify_ack()`£¬Ïò lock_mgr ±©Â¶×î½üÒ»´Î VERIFY ACK¡£
- µ÷Õû ZW101 Á¬ĞøÊ§°Ü½ûÓÃ¼ÆÊı£º`ACK=0x09 (NOT_PRESSED)` ²»ÔÙ¼ÆÈë `fail_streak`£¬±ÜÃâÎó´¥·¢¡°5 ´Î½ûÓÃ¡±¡£
- µ÷Õû lock_mgr Ê§°Ü¼ÆÊı£ºµ±ÈÏÖ¤À´Ô´Îª ZW101 ÇÒ×î½ü ACK Îª `0x09` Ê±£¬Ìø¹ı `fail_count` µİÔö£¬²»½øÈë lockout¡£
- ±£ÁôÖØÊÔÁ´Â·£º`0x09` ÈÔ´¥·¢´°¿ÚÄÚÖØÊÔ£¬µ«²»»á°Ñ¡°Î´°´Ñ¹¡±µ±×÷ÕæÊµÈÏÖ¤Ê§°ÜÀÛ¼Ó¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- menuconfig Ô¤¼ì²éÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114 && bash tools/check_ws63_menuconfig.sh`
- Ô¤¼ì²é½á¹û£ºÊ§°Ü£¨½Å±¾È±Ê§£¬`No such file or directory`£©¡£
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: ZW101 NOT_PRESSED Ê§°ÜÖØÊÔĞŞ¸´£¨±ÜÃâ±¾´°¿Ú¿¨ÔÚ wait_release£©

±ä¸üÕªÒª£º
- ÔÚ ZW101 task ÖĞĞÂÔö¡°×î½üÒ»´Î VERIFY ACK¡±¼ÇÂ¼£¬ÓÃÓÚÖØÊÔÂ·¾¶¾ö²ß¡£
- µ±×î½üÒ»´ÎÊ§°Ü ACK Îª `NOT_PRESSED(0x09)` Ê±£¬Ê§°ÜºóµÄÖØÊÔ¸ÄÎª¡°Á¢¼´ÖØĞÂÇëÇó VERIFY¡±£¬²»ÔÙ½øÈë `wait_release`¡£
- ±£ÁôÔ­ÓĞÀëÊÖºóÖØÊÔ»úÖÆ£º·Ç `NOT_PRESSED` µÄÊ§°ÜÈÔ½øÈë `wait_release`£¬¼ì²âµ½ÀëÊÖºóÔÙÅÅ¶ÓÖØÊÔ¡£
- ÖØÖÃ ARMED ´°¿Ú±£»¤×´Ì¬¡¢ÈÎÎñÆô¶¯Óë reinit Ê±Í¬²½ÇåÀí×î½ü ACK£¬±ÜÃâ¿ç´°¿ÚÑØÓÃ³Â¾É×´Ì¬¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- menuconfig Ô¤¼ì²éÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114 && bash tools/check_ws63_menuconfig.sh`
- Ô¤¼ì²é½á¹û£ºÊ§°Ü£¨½Å±¾È±Ê§£¬`No such file or directory`£©¡£
- ¹¹½¨ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ¹¹½¨½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success`¡£

### 2026-04-13: ZW101 ÀëÊÖÂÖÑ¯½µÔë£¨×´Ì¬±ä»¯´òÓ¡ + Çı¶¯¾²Ä¬ÂÖÑ¯£©

±ä¸üÕªÒª£º
- ÔÚ `wait_release` ½×¶Î°Ñ `CheckSensor` ÂÖÑ¯¼ä¸ô´Ó 120ms µ÷ÕûÎª 400ms£¬½µµÍÎŞĞ§ÂÖÑ¯ÆµÂÊ¡£
- ½«ÀëÊÖ¼ì²âÈÕÖ¾¸ÄÎª¡°½ö×´Ì¬±ä»¯´òÓ¡¡±£¬±ÜÃâ¹Ì¶¨½ÚÅÄÖØ¸´Êä³ö `release_check`¡£
- ÔÚ ZW101 Çı¶¯ÖĞÎªÃüÁîµÈ´ıÁ´Â·ĞÂÔö `trace_silent` ²ÎÊı£¬½ö¶ÔÀëÊÖÂÖÑ¯µÄ `CheckSensor` ÆôÓÃÏêÏ¸ trace ¾²Ä¬¡£
- ±£Áô¹Ø¼üÒµÎñÈÕÖ¾£¨ÀëÊÖºóÖØÊÔÈë¶Ó¡¢VERIFY ³É°Ü¡¢³¬Ê±¸æ¾¯£©£¬²»¸Ä±äÈÏÖ¤ÓïÒå¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: ZW101 È«Á¿Ç¨ÒÆÖØ¹¹£¨ÒÆ³ı¾É ZA£¬»½ĞÑºó VERIFY µÈ´ı½á¹û£©

±ä¸üÕªÒª£º
- °´ `sle_uart_slave` µÄ ZW101 ÒµÎñÄÜÁ¦ÖØ¹¹ `ws63_final`£ºÇı¶¯²ãÖ»±£Áô `ENROLL/VERIFY/ECHO/LIST/DEL/CLEAR/CANCEL`£¬ÒÆ³ı¾É `ZA/RAW` ¼æÈİÂ·¾¶¡£
- ÃÅËø±àÅÅ¸ÄÎª¡°½Ó½ü»½ĞÑºóÆô¶¯ VERIFY ²¢µÈ´ıÈÏÖ¤½á¹û¡±£¬Ê§°ÜÊ±ÔÚ ARMED ×´Ì¬ÏÂÖØ´¥·¢ VERIFY£»Á¬ĞøÊ§°Ü´ïµ½ 5 ´Îºó½ûÓÃ VERIFY ²¢ÉÏ±¨±¨¾¯ÏûÏ¢¡£
- µ÷ÊÔÃüÁîÇ°×ºÍ³Ò»Îª `ZW`£¬ĞÂÃüÁî¼¯ºÏ£º`ZW INIT/STAT/ECHO/VERIFY/ENROLL/LIST/DEL/CLEAR/CANCEL`¡£
- VERIFY Ä¬ÈÏ²ÎÊıÓë slave ¶ÔÆë£º`level=3, id=0xFFFF, param=0x0000`£¬ÈÕÖ¾Êä³öÍ³Ò»Îª `VERIFYING / VERIFY SUCCESS / VERIFY FAIL` ·ç¸ñ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/zw101.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ´úÂë¾²Ì¬¼ì²é£ºÍ¨¹ı£¨¸Ä¶¯ÎÄ¼şÎŞ IDE ±àÒë´íÎó£©¡£
- ¹¹½¨ÑéÖ¤£º¼û±¾´ÎÈÎÎñÊÕÎ²±àÒë½á¹û¼ÇÂ¼¡£

### 2026-04-13: camera ·Ö°üÖØ×éĞŞ¸´£¨¼æÈİ [name,score] °ë°ü£©

±ä¸üÕªÒª£º
- ĞŞ¸´ camera ×Ó¿ÚÊÕ°ü±» WK2114 ·ÖÆ¬Ê±µÄÎóÅĞÎÊÌâ£ºĞÂÔö½ÓÊÕÖØ×é»º³å£¬Ö§³Ö°Ñ `"[Noah_Xiang,0.77"` + `"]"` ÕâÀà°ë°üÆ´³ÉÍêÕûÖ¡ºóÔÙÅĞ¶¨¡£
- »Ø°ü²ú³ö¹æÔòÔö¼Ó `]` ½áÊø·ûÊ¶±ğ£¬²¢¼ÌĞø¼æÈİ `\r\n` ½áÊø·û£¬±ÜÃâĞ­Òé½áÊø·û²»Í³Ò»µ¼ÖÂÂ©ÅĞ¡£
- `score` ½âÎöÔöÇ¿£º³ı `score=0.77 / score:0.77` Íâ£¬ĞÂÔö¼æÈİ `"[name,0.77]"` ½á¹¹£»ÈÔ°´ `score>=0.75` ÅĞ¶¨Í¨¹ı¡£
- ±£³ÖÔ­ÓĞ¹Ø¼ü×ÖÅĞ¶¨£¨pass/success/ok/allow/fail/deny/error/timeout£©×÷Îª¶µµ×£¬²»¸Ä±äÉÏ²ãÃÅËø×´Ì¬»ú½Ó¿Ú¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_camera.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¬ÈÕÖ¾°üº¬ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-13: ÃÅËøÈıÏîÔËĞĞÌ¬ĞŞ¸´Óë¹¹½¨Íê³É

±ä¸üÕªÒª£º
- ÏŞÖÆ»½ĞÑºóµÄ LD2402 ¾àÀëÈÕÖ¾Ë¢ÆÁ£¬±ÜÃâÔÚÃÅËøÓĞĞ§´°¿ÚÄÚ³ÖĞøÊä³öÎŞ¹Ø `distance` ĞÅÏ¢¡£
- ÎÈ¶¨ TTP229 ÃÜÂëÊäÈëÉúÃüÖÜÆÚ£¬±ÜÃâ `123456` ÔÚ×´Ì¬¶¶¶¯ÏÂ±»½Ø¶Ï³Éºó°ë¶ÎÊäÈë¡£
- Ôö¼Ó ZW101 Ö¸ÎÆ×Ô¶¯Ê¶±ğÈÎÎñÓëÇëÇó/È¡ÏûÁ÷³Ì£¬È·±£ÃÅËø½øÈë½Ó½ü´°¿Úºó»áÊµ¼ÊÀ­Æğ¼ì²â¡£
- ĞŞÕıĞÂ¼ÓÈÎÎñÇÅ½Ó´úÂëµÄÈ±Ê§ÒÀÀµ£¬Íê³É `ws63-liteos-app` ¶Ëµ½¶Ë¹¹½¨¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/zw101.h`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾×îºóÏÔÊ¾ `Build target:ws63_liteos_app success` Óë `packet success!`¡£

ºóĞøÊÂÏî£º
- ÉÏ°åºó½¨ÒéÖØµã¹Û²ì»½ĞÑºó LD2402¡¢TTP229¡¢ZW101 ÈıÌõÁ´Â·µÄÏÖ³¡ÈÕÖ¾£¬È·ÈÏÃÅËø×´Ì¬»úÓë¼ì²â´°¿ÚÍêÈ«¶ÔÆë¡£

### 2026-04-12: LD2402 ÃüÁî±ğÃûÊÕÁ²µ½ LD

±ä¸üÕªÒª£º
- ½« `ws63_final` µÄ LD2402 µ÷ÊÔÈë¿ÚÊÕÁ²Îª `LD ...` Ç°×º£¬È¥µô¾É `LD2402 ...` ¼æÈİ±ğÃû¡£
- µ±Ç°¿ÉÖ´ĞĞÃüÁî½ö±£Áô `LD HELP/INIT/STAT/VERSION/SN/MODE/DIST/DELAY/GET/SET/SAVE/GAIN/AUTO/PROGRESS/ALARM/PWR/SAVE3F/RAW/LOG/LOGINT/LOGSTAT`¡£
- Task ²ãÈÔÍ¨¹ı LD2402 Çı¶¯ÊµÏÖĞ­Òé´¦Àí£¬µ«¶ÔÍâµ÷ÊÔÃüÁîÃæÖ»±£Áô `LD`£¬±ÜÃâÏÖ³¡½Å±¾¼ÌĞøÒÀÀµ¾É±ğÃû¡£
- Í¬²½¸üĞÂ `DEBUG_COMMANDS.md`£¬ÒÆ³ı¾ÉÇ°×ºÊ¾ÀıÓë¹ÊÕÏÅÅ²éÖĞµÄ `LD2402` ÃüÁîĞ´·¨¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/Driver/ld2402.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÊ±ÓÅÏÈÈ·ÈÏ `LD INIT`¡¢`LD VERSION`¡¢`LD SN` Óë `LD STAT` Êä³öÊÇ·ñÕı³£¡£
- ÏÖ³¡½Å±¾ÈçÈÔµ÷ÓÃ¾ÉÇ°×º£¬ĞèÒªÖ±½ÓÇĞ»»µ½ `LD`£¬²»ÔÙÒÀÀµ `LD2402` ¼æÈİÈë¿Ú¡£

### 2026-04-12: RGB ÉÏµç²»ÔÙÄ¬ÈÏ½øÈë demo£¬LD2402 ÔËĞĞÌ¬¾àÀëÈÕÖ¾Í³Ò»½ÚÁ÷

±ä¸üÕªÒª£º
- RGB ÈÎÎñÉÏµçºóÖ»×öÓ²¼ş³õÊ¼»¯£¬²»ÔÙÄ¬ÈÏ½øÈëÑİÊ¾Ñ­»·£¬±ÜÃâµÆĞ§ÔÚÎ´ÏÂ·¢ÃüÁîÊ±×Ô¶¯ÅÜÆğÀ´¡£
- ĞÂÔö `WS63_RGB_DEMO_ENABLE_DEFAULT` Ä¬ÈÏ²ßÂÔºê£¬±ãÓÚºóĞø°´ÏÖ³¡ĞèÒªÖØĞÂ´ò¿ªÑİÊ¾Ä£Ê½¡£
- LD2402 µÄ `OFF` / `distance` / ÆÕÍ¨Êı¾İ°üÈÕÖ¾Í³Ò»×ßÍ¬Ò»Ìõ½ÚÁ÷ÅĞ¶Ï£¬±ÜÃâ¾àÀëÎÄ±¾·ÖÖ§ÈÆ¹ıÈÕÖ¾¼ä¸ôÏŞÖÆ¡£
- ±£Áô×î½ü¾àÀëÖµÓë¸üĞÂÊ±¼äË¢ĞÂÂß¼­£¬½öÊÕ½ô´®¿ÚÊä³öÆµÂÊ£¬¼õÉÙÃÅËøÏÖ³¡Ë¢ÆÁ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

### 2026-04-11: TTP229 I2C ÉÏÀ­Èİ´íĞŞ¸´

±ä¸üÕªÒª£º
- ·¢ÏÖ `GPIO15/16` µÄÄÚ²¿ÉÏÀ­ÅäÖÃÔÚµ±Ç°°å¼¶ÉÏ»á·µ»ØÊ§°Ü£¬µ«Õâ²»Ó°Ïì TTP229 µÄ I2C Í¨ĞÅ±¾Éí¡£
- ½« BSP ÖĞµÄÉÏÀ­ÅäÖÃ¸ÄÎª¡°¾¡Á¦ÉèÖÃ¡¢Ê§°Ü½ö¸æ¾¯¡±£¬±ÜÃâÎó°Ñ¿É»Ö¸´µÄÒı½ÅÄÜÁ¦²îÒìµ±³É³õÊ¼»¯Ê§°Ü¡£
- ±£³ÖÔ­ÓĞ I2C ¶ÁÁ÷³Ì¡¢Task ×´Ì¬»úºÍ±¨¾¯Âß¼­²»±ä£¬Ö»ĞŞÕı³õÊ¼»¯Èİ´í±ß½ç¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¬ÇÒ TTP229 BSP ²»ÔÙÒòÄÚ²¿ÉÏÀ­·µ»ØÂëÖ±½ÓÖĞÖ¹³õÊ¼»¯¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÊ±¼ÌĞøÓÅÏÈ¹Û²ì TTP229 µÄ `init ok` / `init fail` ÈÕÖ¾£¬ÔÙÈ·ÈÏ°´¼ü¶ÁÊıÊÇ·ñÎÈ¶¨¡£

### 2026-04-11: TTP229 ¸ÄÎª I2C ¶ÁÈ¡

±ä¸üÕªÒª£º
- ½« `ws63_final` µÄ TTP229 ´Ó¾ÉµÄ SDO/SCL ÖğÎ»É¨Ãè¸ÄÎª±ê×¼ I2C Ö÷»ú¶ÁÁ÷³Ì¡£
- ±£³Ö GPIO16 / GPIO15 ²»±ä£¬¸ÄÎª¶ÔÓ¦ I2C SDA / SCL Òı½Å¸´ÓÃÓëÉÏÀ­ÅäÖÃ¡£
- °´ÊÖ²áÍ³Ò»Îª 2 ×Ö½ÚÖ±½Ó¶ÁÈ¡£¬°´¼üÎ»ÓïÒå±£³ÖÎª `1=°´ÏÂ`¡£
- Task ²ã±£ÁôÔ­ÓĞ×´Ì¬»úÓë¶à¼ü±¨¾¯Âß¼­£¬½öÍ¬²½¸üĞÂ²ÉÑùÄ¬ÈÏÖµÓë´íÎóÈÕÖ¾¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.h`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åºóÓÅÏÈÈ·ÈÏ TTP229 µÄ `INIT/READ/WATCH` ÈÕÖ¾ÓëÊµ¼Ê´¥Ãş½á¹ûÒ»ÖÂ¡£
- ÈôĞèÒª½øÒ»²½½µµÍ I2C ¶ÁÈ¡¶¶¶¯£¬¿É½áºÏ `INT` Òı½ÅÔÙ²¹ÊÂ¼ş»½ĞÑ²ßÂÔ¡£

### 2026-04-11: TTP229 ³ÖĞø¼ì²âÃüÁî + LD2402 ÃüÁîÍ³Ò»

±ä¸üÕªÒª£º
- µ÷ÊÔ´®¿ÚĞÂÔö `TTP229 WATCH ON|OFF` ÃüÁî£¬ÓÃÓÚÆô¶¯/Í£Ö¹¾ØÕó¼üÅÌ³ÖĞø¼ì²âÈÕÖ¾¡£
- ³ÖĞø¼ì²â¸´ÓÃÏÖÓĞ WATCH ÖÜÆÚµ÷¶È£¬ÔÚ¹Ì¶¨½ÚÅÄÏÂÊä³ö `raw/mask/count`£¬±ãÓÚ¹Û²ì°´¼üÊµÊ±±ä»¯¡£
- `ws63_final` ·¶Î§ÄÚ³¹µ×ÒÆ³ı¾ÉÀ×´ïÃüÁîÇ°×º£¬Í³Ò»Îª `LD2402 INIT/RAW/STAT/LOG/LOGINT/LOGSTAT`¡£
- Í¬²½ĞŞÕıÎÄµµÊÖ²áÓë README µÄÃüÁî¿Ú¾¶£¬±£Ö¤ÏÖ³¡Áªµ÷ÃüÁîÓë°ïÖúÊä³öÒ»ÖÂ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°å½¨ÒéÏÈÖ´ĞĞ `TTP229 WATCH ON` ÔÙ°´¼ü£¬È·ÈÏ³ÖĞø¼ì²âÈÕÖ¾Õı³££»Íê³ÉºóÖ´ĞĞ `TTP229 WATCH OFF` ¹Ø±Õ³ÖĞøÊä³ö¡£
- ¾ÉÀ×´ïÖ¸ÁîÇ°×ºÒÑÒÆ³ı£¬ÈçÏÖ³¡½Å±¾ÈÔÊ¹ÓÃ¾ÉÇ°×ºĞèÍ³Ò»Ìæ»»Îª `LD2402 ...`¡£

### 2026-04-11: LD2402 ÉÏµçË¢ÆÁÏŞÖÆ£¨ÔËĞĞÌ¬½ÚÁ÷ + ÔËĞĞÊ±¿ª¹Ø£©

±ä¸üÕªÒª£º
- ĞŞ¸´ `ws63_final` ÖĞ LD2402 ÉÏµçºóÔËĞĞÌ¬ÈÕÖ¾³ÖĞøË¢ÆÁÎÊÌâ£ºÔÚ Driver ²ã¶Ô `LD2402 processing ...` Ôö¼ÓÊ±¼ä½ÚÁ÷£¬Ä¬ÈÏÃ¿ 1000ms ×î¶àÊä³öÒ»´Î¡£
- ±£Áô³õÊ¼»¯/Ê§°ÜÕï¶ÏÈÕÖ¾£¨Èç `init try`¡¢`init failed`£©£¬È·±£Á´Â·¹ÊÕÏ¶¨Î»ÄÜÁ¦²»ÊÜÓ°Ïì¡£
- SLE ÖĞ¼ä¼ş½« `uplink send success` ´Ó¡°Ã¿°ü´òÓ¡¡±¸ÄÎª¡°¿É¿ª¹Ø + ¼ä¸ô½ÚÁ÷ + ÒÖÖÆ¼ÆÊı¡±£¬Ä¬ÈÏ¹Ø±Õ success Öğ°üÈÕÖ¾£¬Ê§°ÜÈÕÖ¾±£³Ö¼´Ê±Êä³ö¡£
- µ÷ÊÔ´®¿ÚĞÂÔöÔËĞĞÊ±¿ØÖÆÃüÁî£º`LD2402 LOG ON|OFF`¡¢`LD2402 LOGINT <ms>`¡¢`LD2402 LOGSTAT`¡¢`SLE ULOG ON|OFF`¡¢`SLE ULOGINT <ms>`¡¢`SLE ULOGSTAT`£¬ÎŞĞèÖØ±àÒë¼´¿ÉÏÖ³¡µ÷½ÚÈÕÖ¾Ç¿¶È¡£
- ÅäÖÃ²ãĞÂÔöÄ¬ÈÏ²ßÂÔºê£¬Í³Ò»¿ØÖÆ LD2402 Óë SLE success ÈÕÖ¾³õÊ¼ĞĞÎª¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/Driver/ld2402.h`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/Middleware/ws63_final_sle.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_sle.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°å¿ÉÏÈÖ´ĞĞ `LD2402 LOGSTAT` Óë `SLE ULOGSTAT` È·ÈÏÄ¬ÈÏ²ßÂÔ£¬ÔÙ°´ÏÖ³¡ĞèÒªÓÃ `LOGINT` ¶¯Ì¬µ÷Õû½ÚÁ÷´°¿Ú¡£
- ÈôĞèÁÙÊ±×¥È«Á¿°ü¼¶ÈÕÖ¾£¬¿É½«¼ä¸ôÉèÎª `0`£¨ÀıÈç `LD2402 LOGINT 0` / `SLE ULOGINT 0`£©¡£

### 2026-04-10: WK2114 Ö÷¿Ú¶ÁÂ·¾¶·À¿¨ËÀĞŞ¸´£¨¹æ±Ü uapi_uart_read ³¤ÂÖÑ¯£©

±ä¸üÕªÒª£º
- ĞŞ¸´ `ws63_final` Ö÷¿Ú UART ¶ÁÂ·¾¶ÔÚµ±Ç°Çı¶¯ÅäÖÃÏÂ¿ÉÄÜ³öÏÖµÄ³¤ÂÖÑ¯¿¨ËÀÎÊÌâ£¬±ÜÃâ `ws63_wk_task` ³ÖĞø 100% Õ¼ÓÃºó´¥·¢ NMI ÖØÆô¡£
- ÔÚ BSP ²ãĞÂÔö¡°°´×Ö½Ú + FIFO ·Ç¿ÕÔ¤ÅĞ + Èí³¬Ê±¡±µÄ°²È«¶ÁÈ¡·â×°£¬Ìæ»»Ö÷¿Ú/µ÷ÊÔ¿ÚÔ­ÓĞÖ±½Ó `uapi_uart_read` µ÷ÓÃ¡£
- ÖØĞ´Ö÷¿Ú `flush_rx` Îª°²È«·Ç×èÈûÖğ×Ö½ÚÇå¿Õ£¬±ÜÃâ `len>1` ¶ÁÈ¡ÔÚÎŞÊı¾İÊ±ÏİÈë²»¿ÉÍË³öÂÖÑ¯¡£
- ±£³Ö `Driver/App` Ğ­ÒéÁ÷³Ì²»±ä£¬½öĞŞ¸´µ×²ã¶ÁÈ¡ÓïÒåÓë³¬Ê±ĞĞÎª¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_uart.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÖØµã¹Û²ì `ld2402_init` ½×¶ÎÊÇ·ñ²»ÔÙ³öÏÖ `ws63_wk_task` ³¤Ê±¼ä 100% Õ¼ÓÃÓë NMI ÖØÆô¡£
- ÈôÏÖ³¡ÈÔÓĞÅ¼·¢³¬Ê±£¬¿ÉÔÙ²¹³ä `ws63_bsp_host_uart_read` µÄ³¬Ê±ÈÕÖ¾£¨¿ªÊ¼/½áÊø/·µ»Ø×Ö½ÚÊı£©ÓÃÓÚ½øÒ»²½¶¨Î»Á´Â·¶¶¶¯¡£

### 2026-04-10: µ÷ÊÔÃüÁîÈÕÖ¾»»ĞĞ¸ñÊ½ĞŞ¸´£¨\r\n ×ÖÃæÁ¿ÎÊÌâ£©

±ä¸üÕªÒª£º
- ĞŞ¸´ `ws63_final` µ÷ÊÔÃüÁîÈÕÖ¾ÖĞ´íÎóÊ¹ÓÃ `\\r\\n` ×ÖÃæÁ¿µÄÎÊÌâ£¬Í³Ò»¸ÄÎª `\r\n` ¿ØÖÆ·ûÊä³ö¡£
- ĞŞ¸´ºó `uart ready` Óë `command list` µÈÈÕÖ¾°´ĞĞÏÔÊ¾£¬²»ÔÙ°Ñ `\r\n` ×÷Îª¿É¼û×Ö·û´òÓ¡¡£
- ±¾´Î½öµ÷ÕûÈÕÖ¾¸ñÊ½£¬²»¸Ä¶¯ÃüÁî½âÎö¡¢Çı¶¯½»»¥ÓëÒµÎñÂß¼­¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°å¹Û²ì `ws63 dbg` Æô¶¯ÈÕÖ¾Ó¦ÖğĞĞ»»ĞĞÏÔÊ¾£»
- ÈôÏÖ³¡´®¿Ú¹¤¾ßÈÔÏÔÊ¾×ªÒåÎÄ±¾£¬Ğè¼ì²éÉÏÎ»»úÊÇ·ñÆôÓÃÁË¡°ÏÔÊ¾¿ØÖÆ×Ö·û×ªÒå¡±Ñ¡Ïî¡£

### 2026-04-10: ZW101 ³õÊ¼»¯³¬Ê±×îĞ¡ĞŞ¸´£¨×Ó¿Ú²¨ÌØÂÊ¶ÔÆë 57600£©

±ä¸üÕªÒª£º
- ĞŞ¸´ `ws63_final` ÖĞ ZW101 ×Ó¿ÚÄ¬ÈÏ²¨ÌØÂÊÓëÄ£×éÄ¬ÈÏÖµ²»Ò»ÖÂµÄÎÊÌâ£º½«×Ó¿Ú1£¨ZW101£©²¨ÌØÂÊ´Ó `115200` ¶ÔÆëµ½ `57600`¡£
- ĞÂÔö ZW101 ³õÊ¼»¯ÅäÖÃÈÕÖ¾£¬Æô¶¯Ê±´òÓ¡ `ZW101 cfg sub-uartX baud=Y`£¬±ãÓÚÏÖ³¡¿ìËÙÈ·ÈÏÅäÖÃÊÇ·ñÉúĞ§¡£
- ±£³ÖÏÖÓĞÎÕÊÖÁ÷³ÌÓë ACK ½âÎö²ßÂÔ²»±ä£¨ÈÔÎª `0x53 -> 0x35 -> check_sensor` Â·¾¶£©£¬È·±£±¾ÂÖ¸Ä¶¯·çÏÕ×îĞ¡¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åºóÖØµãºË¶ÔÈÕÖ¾£º`sub-uart1 init ok, baud=57600` Óë `ZW101 cfg sub-uart1 baud=57600`£»
- ÈôÈÔ³öÏÖ `ack=0x26`£¬ÔÙ½øÈëµÚ¶ş½×¶Î£¨Ë«²¨ÌØÂÊÌ½²â + ½ÓÊÕÍ³¼ÆÈÕÖ¾£©ÅÅ²éÎïÀí²ã²îÒì¡£

### 2026-04-10: ZW101/LD2402 ³õÊ¼»¯Ê§°ÜÕï¶ÏÔöÇ¿£¨ÎÕÊÖÃüÁî²ğ·Ö + Ê§°Ü¹Û²â£©

±ä¸üÕªÒª£º
- ½«µ÷ÊÔÃüÁîÖĞµÄ `ZW101 HANDSHAKE` ´Ó `INIT` ±ğÃû¸ÄÎª¡°½ö·¢ËÍ 0x35 ÎÕÊÖ²¢»ØÏÔ ACK¡±£¬±ãÓÚ¿ìËÙÅĞ¶Ï»ù´¡Á´Â·ÊÇ·ñ¿É´ï¡£
- ĞÂÔö `ZW101 CHECKSENSOR` ÃüÁî£¨0x36£©£¬ÓÃÓÚÓëÎÕÊÖÃüÁîÅäºÏ¶¨Î»¡°ÎÕÊÖ³É¹¦µ«´«¸ĞÆ÷¼ì²âÊ§°Ü¡±µÄ³¡¾°¡£
- ZW101 Çı¶¯²¹³ä³õÊ¼»¯·Ö²½ÈÕÖ¾£ºÃ¿ÂÖ´òÓ¡ `echo/handshake/check_sensor` µÄ `ret` Óë `ack`£¬²¢ÔÚÃüÁîµÈ´ı ACK ³¬Ê±Ê±Êä³öÃüÁîÂë¡£
- LD2402 Çı¶¯²¹³ä³õÊ¼»¯·Ö²½ÈÕÖ¾£ºÃ¿ÂÖ´òÓ¡ `rx_total/valid_frame/enable_ack`£¬ÓÃÓÚÇø·Ö¡°ÍêÈ«ÎŞ»Ø°ü¡±Óë¡°ÊÕµ½Ö¡µ«·ÇÄ¿±ê ACK¡±¡£
- Í¬²½¸üĞÂµ÷ÊÔÊÖ²á£¬ĞÂÔö `ack=0x26`£¨ACK ³¬Ê±£©ÊÍÒåÓëÅÅ²éË³Ğò¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
ºóĞøÊÂÏî£º
- ÉÏ°å½¨ÒéÏÈÖ´ĞĞ `ZW101 HANDSHAKE`¡¢`ZW101 CHECKSENSOR`¡¢`ZW101 ZA ECHO`£¬²¢½áºÏĞÂÔö `init try` ÈÕÖ¾ÅĞ¶ÏÊÇ·ñÎª¡°´®¿ÚÎŞ»Ø°ü¡±¡£
- ¶Ô LD2402 ½¨Òé¹Ø×¢ `init tryN rx_total`£¬ÈôÁ¬ĞøÎª `0`£¬ÓÅÏÈÅÅ²éÄ£¿é¹©µç¡¢TX/RX ½»²æÓë×Ó¿ÚÁ¬Ïß¡£

### 2026-04-12: ÏµÍ³Éè¼Æ¸å¶ÔÆëÏÖÍøÄÜÁ¦

±ä¸üÕªÒª£º
- ÖØĞ´¸ùÄ¿Â¼ [ÏµÍ³Éè¼Æ.md](/home/xixi/code/ÏµÍ³Éè¼Æ.md)£¬½«ÎÄµµ´Ó¸ÅÄî·½°¸ÊÕÁ²Îª¡°ÏÖÍøÄÜÁ¦ + Ñİ½øÂ·Ïß¡±Ë«²ã½á¹¹¡£
- ²¹Æë ws63_final ÒÑÊµÏÖµÄÄ£¿é±ß½ç¡¢ÄÜÁ¦¾ØÕó¡¢ÃÅËø×´Ì¬»ú¡¢ÈÏÖ¤À´Ô´Óëµ÷ÊÔÃüÁî¿Ú¾¶¡£
- Ã÷È· camera ½ö×÷ÎªÍâ²¿ÊÓ¾õÄ£¿éÇÅ½Ó£¬²»ÔÙ°Ñ±¾µØÈËÁ³Ëã·¨Ğ´³É WS63 ÏÖÍøÄÜÁ¦¡£
- ½«±¾µØÃÜÂë¹şÏ£ÈÏÖ¤¡¢¹ØÃÅ¼ì²âµÈÎ´ÂäµØÄÜÁ¦ÒÆ¶¯µ½Ñİ½øÂ·Ïß£¬±ÜÃâÓëÏÖÍøÊµÏÖ»ìĞ´¡£

Ó°ÏìÎÄ¼ş£º
- `/home/xixi/code/ÏµÍ³Éè¼Æ.md`

ÑéÖ¤½á¹û£º
- ¶ÔÕÕ `src/application/mine/ws63_final/README.md`¡¢`src/application/mine/ws63_final/DEBUG_COMMANDS.md` ÒÔ¼°ÏÖÍøÈÎÎñ´úÂëÍê³ÉÈË¹¤ºË¶Ô¡£
- Î´Ö´ĞĞ¹¹½¨£»±¾´Î½öĞŞ¸ÄÉè¼ÆÎÄµµ£¬Î´Éæ¼°Ô´Âë¡£

ºóĞøÊÂÏî£º
- ÈçºóĞø ws63_final Ôö¼ÓÃÜÂëÈÏÖ¤»ò¹ØÃÅ¼ì²â£¬ÔÙ°ÑÑİ½øÂ·ÏßÖĞµÄ¶ÔÓ¦ÌõÄ¿ÌáÉıÎªÏÖÍøÄÜÁ¦ËµÃ÷¡£

### 2026-04-10: ´®¿ÚÊµ²âÈÕÖ¾ÎÊÌâĞŞ¸´£¨RGB Ä¬ÈÏ¿ÉÓÃ + STOP ·ûºÅÏÔÊ¾£©

±ä¸üÕªÒª£º
- ¸ù¾İÏÖ³¡´®¿ÚÈÕÖ¾¶¨Î» `RGB INIT/SET/DEMO` ·µ»Ø `0xffffffff` µÄ¸ùÒò£º`WS63_RGB_ENABLE` ´¦ÓÚ¹Ø±ÕÅäÖÃ¡£
- ½« `WS63_RGB_ENABLE` Ä¬ÈÏÖµ´Ó `0` µ÷ÕûÎª `1`£¬Ê¹ `RGB INIT` µÈµ÷ÊÔÃüÁîÔÚÄ¬ÈÏ¹¹½¨ÏÂ¿ÉÖ±½ÓÁªµ÷¡£
- ĞŞÕıµç»ú×´Ì¬ÈÕÖ¾µÄ RPM ¹éÒ»»¯Âß¼­£ºÔÚ `STOP/BRAKE` ×´Ì¬ÏÂ±£Áô±àÂëÆ÷Ô­Ê¼·ûºÅ£¬±ãÓÚ¹Û²ì·´×ªºó¹ßĞÔË¥¼õ·½Ïò¡£
- ±£³Ö·Ö²ã±ß½ç²»±ä£¬½öĞŞ¸ÄÅäÖÃ²ãÓë Task µ÷ÊÔÏÔÊ¾Âß¼­¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°å½¨Òé¸´²â£º`RGB INIT` -> `RGB SET 255 0 0` -> `RGB DEMO ON`£»
- ·´×ªºóÖ´ĞĞ `MOTOR STOP`£¬È·ÈÏ¶ÌÔİÓà×ªÆÚ¼ä `motor_rpm/out_rps` ·ûºÅÓëÊµ¼Ê·½ÏòÒ»ÖÂ¡£

### 2026-04-09: ZW101 ZA ¼æÈİÃüÁîµ÷ÊÔ½ÓÈë + Ö¸Áî×åº¯Êı²¹Æë

±ä¸üÕªÒª£º
- »ùÓÚ `mine/lib/Ö¸ÎÆÄ£×é²úÆ·ÓÃ»§ÊÖ²á_V1.5.1.pdf` µÄ»ù´¡Í¨ĞÅÁ÷³Ì£¬ÊµÏÖ ZW101 Ğ­ÒéÖ¡×é°ü¡¢ÊÕ°üÌáÖ¡¡¢Ó¦´ğµÈ´ıÓëĞ£ÑéÂ·¾¶¡£
- Çı¶¯²ã²¹Æë ZA ¼æÈİÃüÁîº¯Êı£º`GetEcho(0x53)`¡¢`AutoLogin(0x54)`¡¢`AutoSearch(0x55)`¡¢`SearchResBack(0x56)`¡¢`AutoLoginStabLight(0x57)`¡¢`AutoSearchWithEcho(0x58)`¡¢`ProcessTerminateCmd(0xAA)`¡£
- °´ÊÖ²á²¹ÆëÒµÎñÀà/Î¬»¤Àà/¶¨ÖÆÀàÃüÁîº¯ÊıÊµÏÖ£¨µ±Ç°½×¶ÎÏÈÊµÏÖº¯Êı£¬²»ÔÚÈÎÎñÖ÷Á÷³ÌÄ¬ÈÏµ÷ÓÃ£©¡£
- µ÷ÊÔÃüÁîĞÂÔö `ZW101 ZA` ×ÓÃüÁîÈë¿Ú£¬Ö§³ÖÔÚÏßÁªµ÷²¢´òÓ¡¹Ø¼ü·µ»Ø×Ö¶Î£¨ack/id/score£©¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Driver/zw101.h`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÓÅÏÈÖ´ĞĞ `ZW101 ZA ECHO` Óë `ZW101 ZA SEARCH`£¬È·ÈÏ»Ø°üÊ±ĞòÓë ACK ÂëºÍÊÖ²áÒ»ÖÂ£¬ÔÙÖğ²½Áªµ÷ÒµÎñÀà/Î¬»¤ÀàÃüÁî¡£

### 2026-04-09: ·äÃùÆ÷ÒôÁ¿µ÷½Ú + LD2402/ZW101 µ÷ÊÔÃüÁîÀ©Õ¹

±ä¸üÕªÒª£º
- ·äÃùÆ÷ĞÂÔöÒôÁ¿ÄÜÁ¦£¬ÒôÁ¿²ÎÊıÓ³ÉäÎª PWM Õ¼¿Õ±È£¬Ö§³Ö `0~100%` µ÷½Ú¡£
- µ÷ÊÔ´®¿ÚĞÂÔö `BEEP VOL <0-100>` ÃüÁî£¬×´Ì¬ÈÕÖ¾Ôö¼Ó `vol` ×Ö¶Î¡£
- ĞÂÔö LD2402 µ÷ÊÔÃüÁî£º`INIT/RAW/STAT`£¬Ö§³ÖÔÚÏßÏÂ·¢Ê®Áù½øÖÆÔ­Ê¼Ö¡¡£
- ĞÂÔö ZW101 µ÷ÊÔÃüÁî£º`INIT(HANDSHAKE)/RAW/STAT`£¬Ö§³ÖÎÕÊÖ¸´²âÓëÔ­Ê¼Ö¡Áªµ÷¡£
- ÈÎÎñ²ãĞÂÔöÄ£¿éµ÷ÊÔÇÅ½Ó½Ó¿Ú£¬Í³Ò»ÓÉ App/Task ²ãµ÷ÓÃ Driver£¬Î¬³Ö·Ö²ã±ß½ç¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_beep.c`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.h`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÈôÏÖ³¡°å¿¨·äÃùÆ÷Êä³öÆ«Èõ£¬¿ÉÓÅÏÈÌá¸ß `BEEP VOL`£¬ÔÙ½áºÏ `BEEP FREQ` Î¢µ÷Ìı¸Ğ¡£

### 2026-04-09: ·äÃùÆ÷£¨beep£©ÒÆÖ²µ½ ws63_final

±ä¸üÕªÒª£º
- ĞÂÔö·äÃùÆ÷ BSP ×ÓÄ£¿é£¬·â×° GPIO9/PWM1 µ×²ã³õÊ¼»¯¡¢·¢ÉùÓë¾²Òô¿ØÖÆ¡£
- ĞÂÔö·äÃùÆ÷ Driver ²ã£¬Ìá¹©¿ª¹ØÓëÆµÂÊÓïÒå½Ó¿Ú£¬²¢»º´æµ±Ç°×´Ì¬¡£
- ÈÎÎñ²ãĞÂÔö·äÃùÆ÷³õÊ¼»¯ÓëÈÎÎñ½Ó¿Ú£¬Ö§³ÖÓ¦ÓÃ²ãÍ³Ò»µ÷ÓÃ¡£
- µ÷ÊÔ´®¿ÚÃüÁîĞÂÔö `BEEP ON/OFF/FREQ/STAT`£¬¿ÉÔÚÏß¿Ø²â·äÃùÆ÷¡£
- Í¬²½¸üĞÂµ÷ÊÔÃüÁîÊÖ²á£¬²¹³ä·äÃùÆ÷Áªµ÷ÓëÅÅÕÏËµÃ÷¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_beep.c`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.h`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- Èô°å¿¨·äÃùÆ÷·Ç GPIO9£¬Çë½öµ÷Õû `WS63_BEEP_*` ÅäÖÃºê£¬²»Òª¸Ä Driver/App Âß¼­¡£

### 2026-04-09: µç»úÇı¶¯Óë±àÂëÆ÷²âËÙ½ÓÈë

±ä¸üÕªÒª£º
- ĞÂÔöµç»úÇı¶¯ÄÜÁ¦£ºÕı×ª¡¢·´×ª¡¢»¬ĞĞÍ£Ö¹¡¢É²³µ¼±Í£¡¢Õ¼¿Õ±Èµ÷ËÙ¡£
- ĞÂÔö±àÂëÆ÷²âËÙÄÜÁ¦£ºA/B ÏàÅĞÏò¼ÆÊı£¬ÖÜÆÚ²ÉÑùÊä³ö RPM¡£
- ÔÚÈÎÎñ²ã½ÓÈë motor/encoder ³õÊ¼»¯ÓëÖÜÆÚ²ÉÑù¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/Driver/ws63_motor.h`
- `src/application/mine/ws63_final/Driver/ws63_motor.c`
- `src/application/mine/ws63_final/Driver/ws63_encoder.h`
- `src/application/mine/ws63_final/Driver/ws63_encoder.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/CMakeLists.txt`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨ws63_liteos_app success£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÖØµãÈ·ÈÏ GPIO2 ¶ÔÓ¦ PWM2 µÄÊµ¼Ê¸´ÓÃÊÇ·ñÓë°å¿¨Á¬ÏßÒ»ÖÂ¡£

### 2026-04-09: ÔÚÏß´®¿Ú¿Ø²âÃüÁîÓëÈÕÖ¾ÔöÇ¿

±ä¸üÕªÒª£º
- ĞÂÔöµ÷ÊÔ´®¿ÚÃüÁîÄÜÁ¦£¨Ä¬ÈÏ UART0£ºGPIO17/18£¬115200£©¡£
- Ö§³ÖÃüÁî£º`HELP`¡¢`MOTOR FWD/REV/DUTY/STOP/BRAKE/RPM/STAT/WATCH ON|OFF`¡¢`ENCODER RESET`¡£
- ĞÂÔöÃüÁîÊäÈë¡¢Ö´ĞĞ½á¹û¡¢ÖÜÆÚ¼à¿ØÈıÀàÈÕÖ¾£¬Ö§³ÖÔÚÏß¿Ø²â×·×Ù¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨ws63_liteos_app success£©¡£

ºóĞøÊÂÏî£º
- ÈôÏÖ³¡ÈÕÖ¾¿ÚÒÑÕ¼ÓÃ UART0£¬¿ÉÔÚÅäÖÃÖĞÇĞ»» `WS63_DEBUG_UART_BUS` Óë¶ÔÓ¦Òı½Å¡£

### 2026-04-09: ÈÎÎñÊÕÎ²ÎÄµµÎ¬»¤ Skill ÂäµØ

±ä¸üÕªÒª£º
- ĞÂ½¨ `task-md-maintenance` Skill£¬Ô¼ÊøÃ¿´ÎÈÎÎñÍê³ÉÊ±±ØĞëÎ¬»¤ÖÁÉÙÒ»¸ö Markdown ÎÄµµ¡£
- ÔÚ²Ö¿â¼¶ copilot Ö¸ÁîÖĞ¼ÓÈëÇ¿ÖÆÎÄµµÎ¬»¤¹æÔò£¬²¢Ö¸¶¨ ws63_final Ä¬ÈÏÎ¬»¤ÎÄµµ¡£
- Ã÷È·ÎÄµµÎ¬»¤ÌõÄ¿×îĞ¡×Ö¶Î£º±ä¸üÕªÒª¡¢Ó°ÏìÎÄ¼ş¡¢ÑéÖ¤½á¹û¡¢ºóĞøÊÂÏî¡£

Ó°ÏìÎÄ¼ş£º
- `.github/skills/task-md-maintenance/SKILL.md`
- `.github/copilot-instructions.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114 && git status --short`
- ½á¹û£ºÄ¿±êÎÄ¼ş¾ùÓĞ±ä¸ü²¢¿É×·×Ù£¬¹æÔòÒÑÂäÅÌ¡£

ºóĞøÊÂÏî£º
- ºóĞøÃ¿´ÎÈÎÎñÊÕÎ²¼ÌĞøÔÚ±¾½Ú×·¼Ó¼ÇÂ¼£¬ĞÎ³É¿ÉÉó¼Æ±ä¸ü¹ì¼£¡£

### 2026-04-09: ´®¿Úµ÷ÊÔÃüÁî¶ÀÁ¢ÎÄµµĞÂÔö

±ä¸üÕªÒª£º
- ĞÂÔö¶ÀÁ¢ÎÄµµ `DEBUG_COMMANDS.md`£¬¼¯ÖĞÎ¬»¤´®¿Úµ÷ÊÔÃüÁî¡¢ÈÕÖ¾¸ñÊ½ÓëÁªµ÷Á÷³Ì¡£
- ÔÚÖ÷ README Ôö¼ÓÎÄµµÈë¿Ú£¬±ÜÃâµ÷ÊÔÃüÁî·ÖÉ¢ÔÚ´úÂëÓëÀúÊ·¼ÇÂ¼ÖĞ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114 && git status --short`
- ½á¹û£ºÎÄµµĞÂÔöÓëË÷Òı¸üĞÂ¾ùÒÑÉúĞ§¡£

ºóĞøÊÂÏî£º
- Ã¿´ÎĞÂÔö/µ÷Õû´®¿ÚÃüÁîÊ±Í¬²½¸üĞÂ `DEBUG_COMMANDS.md`£¬±£³ÖÁªµ÷¿Ú¾¶Ò»ÖÂ¡£

### 2026-04-09: ´®¿Úµ÷ÊÔÃüÁîÎÈ¶¨ĞÔĞŞ¸´£¨ÖØ¸´ÈÕÖ¾/Å¼·¢ ERROR£©

±ä¸üÕªÒª£º
- ĞŞ¸´ÃüÁî»»ĞĞ¼æÈİ£º½âÎöÆ÷Í¬Ê±Ö§³Ö `CR`¡¢`LF`¡¢`CRLF`£¬±ÜÃâ¡°Ö»»ØÏÔ²»Ö´ĞĞ¡¢ºóĞøÅúÁ¿Ö´ĞĞ¡±¡£
- ĞŞ¸´ÈÕÖ¾ÖØ¸´£ºµ÷ÊÔÈÕÖ¾Ä¬ÈÏ½öÊä³öµ½µ÷ÊÔ´®¿Ú£¬±ÜÃâÍ¬Ò»ÎïÀí¿Ú±» `osal_printk` Óëµ÷ÊÔ¿ÚË«Ğ´¡£
- ĞŞ¸´ UART ¾ºÕù£ºµ÷ÊÔ´®¿Ú³õÊ¼»¯Ç°ÏÈ `uapi_uart_deinit`£¬½µµÍÓë AT µÈÒÑÓĞ UART ÓÃ»§²¢·¢ÇÀ¶Á·çÏÕ¡£
- ¸üĞÂµ÷ÊÔÊÖ²á£¬²¹³ä´®¿Ú³åÍ»¹æ±ÜºÍÈÕÖ¾¾µÏñ¿ª¹ØËµÃ÷¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÈôÏÖ³¡ÈÔĞèÓë AT ÃüÁî²¢ĞĞ£¬½¨Òé½«µ÷ÊÔÃüÁî¿ÚÇĞ»»µ½¶ÀÁ¢ UART ×ÜÏß²¢µ¥¶À½ÓÏß¡£

### 2026-04-09: µ÷ÊÔÃüÁî½ÓÊÕ¸ÄÎª UART »Øµ÷+ĞĞ¶ÓÁĞ£¬×´Ì¬ÈÕÖ¾¼ò»¯

±ä¸üÕªÒª£º
- µ÷ÊÔÃüÁî½ÓÊÕ´Ó¡°Ö÷Ñ­»·ÂÖÑ¯ read¡±¸ÄÎª¡°UART ÖĞ¶Ï»Øµ÷×éÖ¡ + ĞĞ»º³å¶ÓÁĞ³ö¶ÓÖ´ĞĞ¡±£¬½µµÍÃüÁî¶ªÊ§ÓëÑÓ³Ù¡£
- ĞÂÔö¶ÓÁĞÒç³ö/ÃüÁî³¬³¤/½ÓÊÕ´íÎó¸æ¾¯£¬±ÜÃâ¸ßÈÕÖ¾¸ºÔØ³¡¾°ÏÂ¾²Ä¬¶ªÃüÁî¡£
- ×´Ì¬¿ìÕÕÈÕÖ¾¸ÄÎª½öÊä³ö·½ÏòÓë×ªËÙ£¨`dir` + `rpm`£©£¬²¢°´µ±Ç°µç»ú×´Ì¬¹æ·¶»¯ RPM ·ûºÅ¡£
- Í¬²½¸üĞÂ `DEBUG_COMMANDS.md` µÄ½ÓÊÕ»úÖÆÓëÈÕÖ¾¸ñÊ½ËµÃ÷¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÈôÏÖ³¡ÃüÁîÓë WATCH ²¢·¢ºÜ¸ß£¬½¨ÒéÏÈ `MOTOR WATCH OFF` ÔÙÁ¬ĞøÏÂ·¢¿ØÖÆÃüÁî¡£

### 2026-04-09: ×´Ì¬ÈÕÖ¾Ôö¼Óµç»úÖá RPM ÓëÊä³öÖá RPS Í¬Ê±Êä³ö

±ä¸üÕªÒª£º
- ×´Ì¬¿ìÕÕÈÕÖ¾´Óµ¥Ò» `rpm` À©Õ¹Îª `motor_rpm` Óë `out_rps` Á½¸ö×Ö¶Î£¬±ãÓÚÍ¬Ê±¹Û²ìµç»úÖáÓëÊä³öÖáËÙ¶È¡£
- ĞÂÔö¼õËÙ±ÈÅäÖÃºê `WS63_MOTOR_GEAR_RATIO`£¨Ä¬ÈÏ 150£©£¬Êä³öÖá `rps` °´¸Ã²ÎÊı»»Ëã¡£
- ĞŞÕı¸ºĞ¡ÊıÏÔÊ¾±ß½ç£¬Ö§³Ö `-0.xxx` ĞÎÊ½µÄÊä³öÖá×ªËÙÈÕÖ¾¡£
- Í¬²½¸üĞÂ `DEBUG_COMMANDS.md` µÄÈÕÖ¾¸ñÊ½Óë»»ËãËµÃ÷¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- Èô¼õËÙÏä¹æ¸ñ±ä»¯£¬½öĞèµ÷Õû `WS63_MOTOR_GEAR_RATIO` ¼´¿ÉÍ¬²½ĞŞÕı `out_rps`¡£

### 2026-04-09: Task Óë BSP °´¹¦ÄÜÄ£¿é²ğ·ÖÖØ¹¹

±ä¸üÕªÒª£º
- ½« `ws63_final_task.c` ÖĞ´®¿Úµ÷ÊÔÃüÁîÊµÏÖ²ğ·Öµ½¶ÀÁ¢×ÓÄ£¿é£¬Ö÷ÈÎÎñ½ö±£Áôµ÷¶ÈÓëµ÷ÓÃÈë¿Ú¡£
- ½« `ws63_final_bsp.c` °´¹¦ÄÜ²ğÎª UART¡¢RGB/SPI¡¢µç»ú/±àÂëÆ÷Èı¸ö×ÓÄ£¿é£¬Ö÷ BSP ÎÄ¼ş½ö±£ÁôÍ¨ÓÃ¿ØÖÆÄÜÁ¦¡£
- ¸üĞÂ CMake Ô´ÎÄ¼şÁĞ±í£¬È·±£²ğ·ÖºóÄ£¿é²ÎÓëÍ³Ò»¹¹½¨£¬²»¸Ä±äÏÖÓĞ¶ÔÍâ½Ó¿Ú¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_uart.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_rgb.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_motor_encoder.c`
- `src/application/mine/ws63_final/CMakeLists.txt`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÈôºóĞø¼ÌĞøÀ©Õ¹µ÷ÊÔÃüÁî£¬ÓÅÏÈĞŞ¸Ä `App/Task/ws63_final_task_debug.c`£¬±ÜÃâÖ÷ÈÎÎñÎÄ¼şÔÙ´ÎÅòÕÍ¡£

### 2026-04-09: ·Ö²ã±ß½çÕû¸Ä£¨App Ô½²ãÓë Driver ·´ÏòÒÀÀµÊÕ¿Ú£©

±ä¸üÕªÒª£º
- ĞÂÔöµ÷ÊÔ UART Çı¶¯ÃÅÃæ `ws63_debug_uart`£¬ÓÉ Driver ²ã³Ğ½Óµ÷ÊÔ´®¿Ú³õÊ¼»¯/·¢ËÍ/»Øµ÷×¢²á£¬App ²»ÔÙÖ±½Óµ÷ÓÃ BSP µ÷ÊÔ UART ½Ó¿Ú¡£
- Middleware OSAL ĞÂÔö `ws63_os_irq_lock/ws63_os_irq_unlock/ws63_os_feed_watchdog`£¬ÓÃÓÚ³Ğ½Ó App µÄÁÙ½çÇøÓëÎ¹¹·ĞèÇó£¬±ÜÃâ App Ö±Á¬ RTOS/HAL Ô­Óï¡£
- App Ö÷ÈÎÎñ½« `uapi_watchdog_kick` Ìæ»»Îª `ws63_os_feed_watchdog`£¬²¢ÒÆ³ı¶Ô `watchdog.h` Óë `ws63_final_bsp.h` µÄÖ±½ÓÒÀÀµ¡£
- `zw101`¡¢`ld2402` Çı¶¯È¥³ı¶Ô `ws63_final_osal` µÄ·´ÏòÒÀÀµ£¬ÑÓÊ±µ÷ÓÃÍ³Ò»¸ÄÎª `ws63_bsp_sleep_ms`£¬»Ö¸´ Driver -> BSP µ¥ÏòÒÀÀµ¡£
- ¸üĞÂ `CMakeLists.txt`£¬½«ĞÂÇı¶¯Ô´ÎÄ¼şÄÉÈë±àÒë¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Driver/ws63_debug_uart.h`
- `src/application/mine/ws63_final/Driver/ws63_debug_uart.c`
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && find output/ws63 -type f \( -name 'ws63_final_task.c.obj' -o -name 'ws63_final_task_debug.c.obj' -o -name 'ws63_debug_uart.c.obj' \)`
- ½á¹û£ºµ±Ç°Êä³öÄ¿Â¼Î´ÃüÖĞÉÏÊö¶ÔÏóÎÄ¼ş£»½áºÏµ±Ç° menuconfig ×´Ì¬£¬`ws63_final` Ä£¿éÎ´±»ÄÉÈë±¾´ÎÄ¿±ê¾µÏñ±àÒëÁ´Â·¡£

ºóĞøÊÂÏî£º
- ÈôĞè¶Ô±¾´Î¸Ä¶¯×ö¡°ÕæÊµ±àÒëÁ´Â·¡±ÑéÖ¤£¬ÇëÏÈÔÚ menuconfig ÖĞÖØĞÂÆôÓÃ `CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED` ºóÔÙ¹¹½¨¡£

### 2026-04-09: Task Ö÷ÎÄ¼ş°´¹¦ÄÜ²ğ·Ö£¨ºËĞÄµ÷¶È/Éè±¸¿ØÖÆ/RGB/´«¸ĞÆ÷ÇÅ½Ó£©

±ä¸üÕªÒª£º
- ½« `ws63_final_task.c` ÊÕÁ²Îª¡°ºËĞÄµ÷¶È + ×Ó¿Ú³õÊ¼»¯ + Ö÷Ñ­»·¡±£¬ÒÆ³ıµç»ú/·äÃùÆ÷¡¢RGB¡¢´«¸ĞÆ÷ÇÅ½ÓµÈÊµÏÖÏ¸½Ú¡£
- ĞÂÔö `ws63_final_task_internal.h` ×÷Îª Task ÄÚ²¿Ä£¿é¹²Ïí½Ó¿Ú£¬Í³Ò»ÉùÃ÷ÄÚ²¿ÄÜÁ¦³õÊ¼»¯Óë×´Ì¬²éÑ¯½Ó¿Ú¡£
- ĞÂÔö `ws63_final_task_device_ctrl.c`£¬¼¯ÖĞ³ĞÔØµç»ú/±àÂëÆ÷/·äÃùÆ÷³õÊ¼»¯Óë¿ØÖÆ API¡£
- ĞÂÔö `ws63_final_task_rgb.c`£¬¶ÀÁ¢Î¬»¤ RGB ÑİÊ¾×´Ì¬»úÓëÖÜÆÚÇı¶¯Âß¼­¡£
- ĞÂÔö `ws63_final_task_sensor_bridge.c`£¬¶ÀÁ¢Î¬»¤ LD2402/ZW101 µ÷ÊÔÇÅ½Ó½Ó¿Ú¡£
- ¸üĞÂ `CMakeLists.txt` Ô´Çåµ¥£¬½«²ğ·ÖÎÄ¼şÄÉÈë `mine_ws63_final` ×é¼ş±àÒë¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_device_ctrl.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- ½á¹û£ºclean+È«Á¿¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©£¬²¢Éú³É²ğ·Ö¶ÔÏóÎÄ¼ş£º
	`ws63_final_task.c.obj`¡¢`ws63_final_task_device_ctrl.c.obj`¡¢`ws63_final_task_rgb.c.obj`¡¢`ws63_final_task_sensor_bridge.c.obj`¡£

ºóĞøÊÂÏî£º
- ÈôºóĞø¼ÌĞøÀ©Õ¹ÒµÎñ£¬ÇëÓÅÏÈÔÚ¶ÔÓ¦×ÓÄ£¿éÎÄ¼şÖĞĞÂÔöÂß¼­£¬±ÜÃâÔÙ´Î»ØÁ÷µ½Ö÷ÈÎÎñÎÄ¼ş¡£

### 2026-04-09: ĞÂÔö RGB ÔÚÏßµ÷ÊÔÃüÁî£¨INIT/SET/OFF/DEMO/STAT£©

±ä¸üÕªÒª£º
- Task ²ãĞÂÔö RGB ¿ØÖÆ½Ó¿Ú£ºÇı¶¯ÖØ³õÊ¼»¯¡¢ÊÖ¶¯ÉèÉ«¡¢¹ØµÆ¡¢ÑİÊ¾Ä£Ê½¿ª¹Ø¡¢×´Ì¬²éÑ¯¡£
- RGB ×ÓÄ£¿éĞÂÔöÑİÊ¾¿ª¹Ø×´Ì¬¹ÜÀí£ºÊÖ¶¯ÉèÉ«ºó×Ô¶¯¹Ø±ÕÑİÊ¾£¬±ÜÃâÑÕÉ«±»ÖÜÆÚÈÎÎñ¸²¸Ç¡£
- µ÷ÊÔÃüÁîĞÂÔö `RGB INIT`¡¢`RGB SET <R> <G> <B>`¡¢`RGB OFF`¡¢`RGB DEMO ON|OFF`¡¢`RGB STAT`¡£
- `HELP` Êä³öÓëµ÷ÊÔÊÖ²áÍ¬²½²¹Æë RGB ÃüÁîËµÃ÷Óë²ÎÊıÔ¼Êø¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÈôÏÖ³¡ĞèÒª¡°¹Ì¶¨É«³£ÁÁ¡±£¬½¨ÒéÏÈÖ´ĞĞ `RGB DEMO OFF`£¬ÔÙÖ´ĞĞ `RGB SET R G B`¡£

### 2026-04-10: ws63_final RTOS ¶àÈÎÎñ»¯¸ÄÔì£¨WK2114/SLE/RGB/BEEP ½âñî£©

±ä¸üÕªÒª£º
- ½«Ô­ `ws63_final_task.c` µ¥Ñ­»·ÖØ¹¹Îª¡°¹ÜÀíÈÎÎñ + WK2114 Í¨ĞÅÈÎÎñ + SLE Ğ­ÒéÈÎÎñ¡±£¬²¢ÒıÈëÈÎÎñ¼äÏûÏ¢¶ÓÁĞ¡£
- `WK2114` Óë `SLE` Ö®¼ä¸ÄÎª¶ÓÁĞÇÅ½Ó£ºÉÏĞĞÓÉ WK2114 Í¶µİµ½ SLE ¶ÓÁĞ£¬ÏÂĞĞÓÉ SLE »Øµ÷Í¶µİµ½ WK2114 ·¢ËÍ¶ÓÁĞ¡£
- `RGB` ×ÓÄ£¿é¸ÄÎª¶ÀÁ¢ RTOS ÈÎÎñ£¬ĞÂÔö¿ØÖÆ¶ÓÁĞ£¬`SET/OFF/DEMO/REINIT` ÃüÁîÓÉ¶ÓÁĞ´®ĞĞÖ´ĞĞ£¬±ÜÃâÓëÆäËüÄ£¿éÇÀÕ¼Ö´ĞĞÁ´Â·¡£
- `BEEP` ×ÓÄ£¿é¸ÄÎª¶ÀÁ¢ RTOS ÈÎÎñ£¬ĞÂÔö¿ØÖÆ¶ÓÁĞ£¬`ON/OFF/VOL` ÃüÁîÓÉÈÎÎñ´®ĞĞÂäµØ£¬¼õÉÙ²¢·¢Ó²¼ş·ÃÎÊ³åÍ»¡£
- ÖĞ¼ä¼ş `ws63_final_osal` ĞÂÔöÏûÏ¢¶ÓÁĞ·â×°½Ó¿Ú£¬±£³Ö App ²ã²»Ö±½ÓÒÀÀµµ×²ã OSAL ¶ÓÁĞÏ¸½Ú¡£
- ÅäÖÃ²ãĞÂÔö¶àÈÎÎñ²ÎÊı£¨Õ»´óĞ¡¡¢ÓÅÏÈ¼¶¡¢¶ÓÁĞÉî¶È£©£¬Ä¬ÈÏ±£ÁôÏÖÓĞĞĞÎª²¢Ö§³ÖºóĞø°´¸ºÔØµ÷²Î¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_device_ctrl.c`
- `src/application/mine/ws63_final/App/Main/ws63_final_main.c`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÈôÏÖ³¡²¢·¢¸ºÔØ³ÖĞøÉı¸ß£¬¿ÉÓÅÏÈµ÷´ó `WS63_WK2114_TX_QUEUE_DEPTH` Óë `WS63_SLE_UPLINK_QUEUE_DEPTH`¡£
- Èô·¢ÏÖÈÎÎñÕ»Ë®Î»Æ«µÍ£¬½¨ÒéÏÈÌá¸ß `WS63_SLE_TASK_STACK_SIZE`£¬ÔÙÆÀ¹À `WS63_WK2114_TASK_STACK_SIZE`¡£

### 2026-04-11: TTP229 ´¥Ãş¼üÅÌ½ÓÈë ws63_final£¨GPIO16/15£©

±ä¸üÕªÒª£º
- ĞÂÔö TTP229 BSP/Driver/Task Èı²ãÊµÏÖ£¬×ñÑ­ `ws63_final` ·Ö²ã¼Ü¹¹£¬Ó²¼ş²Ù×÷½ö·ÅÔÚ BSP¡£
- ¹Ì¶¨½ÓÏßÅäÖÃ£º`SCL=GPIO16`¡¢`SDO(°åÉÏ±ê×¢ SDA)=GPIO15`£¬²¢ÔÚÅäÖÃ²ãĞÂÔöÊ±ĞòÓëÈÎÎñ²ÎÊıºê¡£
- ĞÂÔö TTP229 ¶ÀÁ¢ÈÎÎñÓë×´Ì¬»ú£¨`INIT/DISABLED/READY/FAULT`£©£¬Ö§³ÖÔËĞĞÊ±ÆôÍ£ÓëÖØ³õÊ¼»¯¡£
- Í³Ò»°´¼üÓïÒåÎª¡°Î»Îª 1 ±íÊ¾°´ÏÂ¡±£¬²¢ĞÂÔö¡°¶à¼üÍ¬Ê±°´ÏÂ±¨¾¯¡±»úÖÆ¡£
- µ÷ÊÔ´®¿ÚĞÂÔö `TTP229 INIT/STAT/READ/MASK/WATCH/ENABLE/ALARM` ÃüÁî£¬±ãÓÚÏÖ³¡Áªµ÷ÓëÅÅÕÏ¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.h`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÖØµãÑéÖ¤ `TTP229 READ` ·µ»ØÎ»Í¼ÊÇ·ñÂú×ã¡°Î»1=°´ÏÂ¡±ÓïÒå¡£
- Èô `mask` ³¤ÆÚ¹Ì¶¨²»±ä£¬ÓÅÏÈ¼ì²é GPIO15/16 ½ÓÏßÓë TTP229 Ê±Ğò²ÎÊı¡£

### 2026-04-11: TTP229 Êµ²âÓ³ÉäÂäµØ

±ä¸üÕªÒª£º
- °´ÏÖ³¡Êµ²âÈ·ÈÏ TTP229 raw Ó³Éä£ºA=0x1000¡¢B=0x2000¡¢C=0x4000¡¢D=0x8000¡¢1=0x0001¡¢2=0x0010¡¢3=0x0100¡¢*=0x0008¡¢0=0x0080¡¢#=0x0800¡£
- ¶à¼üÍ¬Ê±°´ÏÂ±£³Ö°´Î»Ïà¼ÓÓïÒå£¬µ÷ÊÔÊä³öĞÂÔö `keys=` ¿É¶Á±êÇ©£¬±ãÓÚÖ±½Ó¶ÔÕÕÎïÀí¼üÎ»¡£
- BSP È¥µôÄÚ²¿ÉÏÀ­³¢ÊÔ£¬±£³ÖÖ»Ê¹ÓÃ°åÉÏÍâÖÃÉÏÀ­£¬²»ÔÙ°Ñ¿É»Ö¸´µÄÒı½Å²îÒìµ±³É³õÊ¼»¯Â·¾¶µÄÒ»²¿·Ö¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£º¹¹½¨Í¨¹ı£¨`Build target:ws63_liteos_app success`£©¡£

ºóĞøÊÂÏî£º
- ÉÏ°åÊ±ÓÅÏÈÑéÖ¤ `TTP229 READ` / `TTP229 WATCH ON` µÄ `keys=` Êä³öÊÇ·ñÓë A/B/C/D/1/2/3/*/0/# Ò»ÖÂ¡£
- ÈçºóĞø²¹³ä¸ü¶à°´¼ü±êÇ©£¬Ö»ĞèÀ©Õ¹ Task ²ãÓ³Éä±í£¬²»Ó°Ïì BSP ¶ÁÂë¡£

### 2026-04-11: ÖÇÄÜÃÅËø±àÅÅ¹Ç¼ÜÆô¶¯£¨LD2402 distance + camera ×Ó¿Ú£©

±ä¸üÕªÒª£º
- LD2402 Çı¶¯¿ªÊ¼½âÎö `distance:xxx` ÎÄ±¾Êä³ö£¬²¢°Ñ×î½üÒ»´Î¾àÀëÖµ±©Â¶¸øÈÎÎñ²ã¡£
- ĞÂÔö camera ÈÎÎñ£¬Í³Ò»°ÑÒµÎñÎÄ±¾·â×°Îª `[camera]xxx` ºóÍ¨¹ı WK2114 À©Õ¹´®¿Ú 3 / 115200 ·¢ËÍ¡£
- ĞÂÔöÃÅËø±àÅÅÈÎÎñ¹Ç¼Ü£º°´ LD2402 ¾àÀëãĞÖµ½øÈë½Ó½ü´°¿Ú£¬²¢½ÓÊÕ camera ÈÏÖ¤½á¹ûÇı¶¯¿ªËø/Ëø¶¨×´Ì¬£»ÈÏÖ¤´°¿ÚÄ¬ÈÏ 20s£¬±ãÓÚ¶ÌÔİÍ£¶Ùºó¼ÌĞøÍê³ÉÊäÈë»òÊ¶±ğ¡£
- µ÷ÊÔ´®¿ÚĞÂÔö `LD2402 DIST`£¬¿ÉÖ±½Ó²é¿´×î½üÒ»´Î¾àÀëÖµÓë¸üĞÂÊ±¼ä¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/Driver/ld2402.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_camera.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`python3 /home/xixi/code/fbb_ws63_20260114/src/build.py -c ws63-liteos-app`
- ½á¹û£º±¾´Î¸Ä¶¯Éæ¼°µÄÎÄ¼ş¾²Ì¬¼ì²éÍ¨¹ı£»ÍêÕû¹¤³Ì¹¹½¨ÈÔ±»²Ö¿âÄÚ¼ÈÓĞµÄ driver/pwm¡¢driver/i2c¡¢hal/efuse¡¢main.c µÈ¶ÀÁ¢´íÎó×è¶Ï¡£

ºóĞøÊÂÏî£º
- ZW101 ÏÖÔÚ»áÔÚ½Ó½ü´°¿ÚÄÚÓÉ¶ÀÁ¢ÈÎÎñ´¥·¢×Ô¶¯Ê¶±ğ£»TTP229 »á±£ÁôÇ°×ºÊäÈë²¢ÔÚ `#` Ìá½»Ê±ÔÙ×ö armed ÅĞ¶¨¡£
- ´ıÈ·ÈÏÍâ²¿ camera Ä£¿éµÄ»Ø°ü¹Ø¼ü×Öºó£¬¿É°Ñ `pass/fail` ÅĞ¶¨ÊÕÁ²µÃ¸üÑÏ¸ñ¡£

### 2026-04-13: ZW101 ĞøÃü´°¿ÚÍ³Ò» + µ÷ÊÔÆô¶¯ÈÕÖ¾¾²Ä¬

±ä¸üÕªÒª£º
- ½«ÃÅËø½Ó½ü»½ĞÑ´°¿ÚÄ¬ÈÏÖµ´Ó 20s µ÷ÕûÎª 10s£¬²¢ĞÂÔöÍ³Ò»µÄ´°¿ÚĞøÃü½Ó¿Ú¡£
- camera¡¢TTP229¡¢ZW101 Ê¶±ğ½á¹ûÓë LD2402 ¾àÀëĞ¡ÓÚ 80mm µÄÓĞĞ§ÊäÈë¶¼»áË¢ĞÂ»½ĞÑ´°¿Ú¡£
- È¥µôµ÷ÊÔ´®¿Ú³õÊ¼»¯Ê±×Ô¶¯´òÓ¡µÄ `[ws63 dbg]` HELP ÁĞ±íÓëÃüÁî»ØÏÔ£¬±£ÁôÊÖ¶¯ HELP ÓëÃüÁî½á¹ûÈÕÖ¾¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_camera.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`

### 2026-04-14: ZW101 ACK_TIMEOUT ÉúÃüÖÜÆÚÄÚ×Ô¶¯ÖØÀ­ VERIFY

±ä¸üÕªÒª£º
- µ÷Õû ZW101 VERIFY ³¬Ê±ÓïÒå£ºµ± `ACK_TIMEOUT` ·¢ÉúÔÚµ±Ç° ARMED ÉúÃüÖÜÆÚÄÚÊ±£¬ÈÎÎñ²ãÏÈ×Ô¶¯ÖØÀ­ VERIFY£¬²»ÔÙ°ÑÕâ´Î³¬Ê±Ö±½ÓÉÏ±¨¸ø lock_mgr¡£
- ĞÂÔö ZW101 ¶ÀÁ¢µÄ³¬Ê±ÖØÊÔ¼ÆÊı `timeout_retry`£¬Óë `fail_streak` ·ÖÀë£¬±ÜÃâ°ÑÍ¨ĞÅ³¬Ê±ºÍÕæÊµÊ¶±ğÊ§°Ü»ìÎªÒ»Àà¡£
- ±£ÁôÔ­ÓĞÀëÊÖÖØÊÔÓëÊ§°Ü½ûÓÃÂß¼­£ºÖ»ÓĞ³¬Ê±ÖØÊÔºÄ¾¡ºó£¬²Å»ØÂäµ½ÆÕÍ¨Ê§°ÜÍ³¼ÆºÍ lockout ÓïÒå¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `######### Build target:ws63_liteos_app success` Óë `packet success!`¡£

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && CMAKE_BUILD_PARALLEL_LEVEL=1 python3 build.py -c ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¬`Build target:ws63_liteos_app success`¡£

### 2026-04-14: µ÷ÊÔÃüÁî¸ÄÎªÖ÷»ú SLE µ¥Èë¿Ú + µ÷ÊÔÈÕÖ¾ SLE »Ø´«

±ä¸üÕªÒª£º
- ½« `ws63_final` µ÷ÊÔÃüÁîÈë¿ÚÇĞ»»ÎªÖ÷»ú SLE ÏÂĞĞ×¢Èë£¬ÏÈÏû·ÑÎÄ±¾ÃüÁî£¬Î´ÃüÖĞÔÙ×ßÔ­×Ó¿ÚÍ¸´«¡£
- µ÷ÊÔÈÕÖ¾Êä³öÂ·¾¶µ÷ÕûÎª SLE ÉÏĞĞ `DEBUG` ±êÇ©£¬`ws63_final` ±¾»úµ÷ÊÔ UART Ä¬ÈÏ²»ÔÙ³Ğµ£µ÷ÊÔÈÕÖ¾Êä³ö¡£
- µ÷ÊÔÄ£¿é¿ª¹Ø²ğ·ÖÎª¡°ÃüÁîºËĞÄ¿ª¹Ø + ±¾»ú UART I/O ¿ª¹Ø + SLE ÃüÁî/ÈÕÖ¾¿ª¹Ø¡±£¬±ÜÃâ¹Ø±Õ±¾»ú´®¿ÚºóÃüÁîºËĞÄÊ§Ğ§¡£
- Í¬²½¸üĞÂµ÷ÊÔÊÖ²á£¬Ã÷È·¡°ÃüÁîÓëµ÷ÊÔÈÕÖ¾¾ù¾­ SLE£¬ws63_final ±¾»ú´®¿ÚÄ¬ÈÏÎŞµ÷ÊÔÊä³ö¡±¡£

Ó°ÏìÎÄ¼ş£º
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_sle.c`
- `src/application/mine/ws63_final/Middleware/ws63_final_sle.h`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

ÑéÖ¤½á¹û£º
- ÃüÁî£º`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- ½á¹û£ºÍ¨¹ı£¬ÈÕÖ¾°üº¬ `######### Build target:ws63_liteos_app success` Óë `packet success!`¡£

### 2026-04-14: ARMED å‘¨æœŸå†…æ”¹ä¸ºæ¥æºçº§ç‹¬ç«‹å°ç¦ï¼ˆæŒ‡çº¹/æŒ‰é”®ï¼‰

å˜æ›´æ‘˜è¦ï¼š
- é‡æ„é—¨é”ç¼–æ’ä»»åŠ¡ lock_mgrï¼šå–æ¶ˆè·¨æ¥æºå…¨å±€å¤±è´¥è®¡æ•°ä¸ LOCKOUT è§¦å‘ï¼Œæ”¹ä¸ºåªå¤„ç† ARMED ç”Ÿå‘½å‘¨æœŸä¸ç»Ÿä¸€åé¦ˆã€‚
- ä¿ç•™æ¥æºä»»åŠ¡å„è‡ªçš„ 5 æ¬¡å¤±è´¥å°ç¦ç­–ç•¥ï¼šæŒ‡çº¹å¤±è´¥ 5 æ¬¡ä»…ç¦ç”¨æŒ‡çº¹ã€æŒ‰é”®å¤±è´¥ 5 æ¬¡ä»…ç¦ç”¨æŒ‰é”®ï¼Œç›´åˆ°æœ¬æ¬¡ ARMED å‘¨æœŸç»“æŸã€‚
- ç»Ÿä¸€è®¤è¯å¤±è´¥çš„å£°å…‰æç¤ºé“¾è·¯ï¼šåœ¨ lock_mgr å¯¹æœ‰æ•ˆå¤±è´¥äº‹ä»¶ç»Ÿä¸€è§¦å‘èœ‚é¸£ + RGB çº¢ç¯æç¤ºã€‚
- ä¸ºé¿å…åŒæç¤ºï¼Œç§»é™¤ TTP229 æœ¬åœ°å¤±è´¥èœ‚é¸£ï¼Œç”± lock_mgr ç»Ÿä¸€è´Ÿè´£å¤±è´¥æç¤ºã€‚
- é¡ºå¸¦ä¿®å¤ sensor_bridge é¡¶éƒ¨å†å²æŸåç‰‡æ®µï¼ˆä¹±ç æ³¨é‡Š/å­¤ç«‹è¯­å¥/ç¼ºå¤±å®å®šä¹‰ï¼‰ï¼Œæ¢å¤å¯ç¼–è¯‘æ€§ï¼Œä¸æ”¹ä¸šåŠ¡ç­–ç•¥ã€‚

å½±å“æ–‡ä»¶ï¼š
- src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c
- src/application/mine/ws63_final/README.md

éªŒè¯ç»“æœï¼š
- å‘½ä»¤ï¼šcd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app
- ç»“æœï¼šé€šè¿‡ï¼Œæ—¥å¿—åŒ…å« ######### Build target:ws63_liteos_app successã€‚

é£é™©/åç»­ï¼š
- lock_mgr å·²ä¸å†è¿›å…¥å…¨å±€ LOCKOUTï¼›è‹¥åç»­éœ€è¦â€œæ¥æºçº§å°ç¦ + å…¨å±€é”å®šå¹¶å­˜â€ç­–ç•¥ï¼Œéœ€è¦å•ç‹¬è®¾è®¡ä¼˜å…ˆçº§ä¸å†²çªå¤„ç†è§„åˆ™ã€‚

### 2026-04-14: LOCK Event Uplink Enhancement (key/finger/camera)

Change Summary:
- Added dedicated LOCK business uplink tag routing via SLE subport 4 -> [LOCK].
- Added lock event report on disable edge after 5 consecutive failures:
  - key path: result=locked;source=key;reason=ttp229_fail_5
  - finger path: result=locked;source=finger;reason=zw101_fail_5
- Added unlock success report for all auth sources:
  - key success: result=unlock_ok;source=key
  - finger success: result=unlock_ok;source=finger;finger_id=<id>;score=<score>
  - camera success: result=unlock_ok;source=camera;camera_label=<label>
- LOCK business event posting is sent through a dedicated API path and is not gated by DEBUG INIT.

Affected Files:
- src/application/mine/ws63_final/Config/ws63_final_config.h
- src/application/mine/ws63_final/Middleware/ws63_final_sle.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h
- src/application/mine/ws63_final/App/Task/ws63_final_task.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c
- src/application/mine/ws63_final/App/Task/ws63_final_task_camera.c

Verification:
- Build command:
  cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app
- Build result:
  Build target:ws63_liteos_app success
  packet success!

Risks / Follow-up:
- Host side parser/display can optionally add [LOCK] specific pretty-printing while keeping raw forwarding unchanged.
- If lock event format evolves, keep key=value schema backward compatible for host tools.

## 2026-04-14 Maintenance Log
- **ä¿®æ”¹æ‘˜è¦**ï¼šé‡æ„ä¼˜åŒ– `ws63_final` é—¨é”ç›¸å…³ä»»åŠ¡ï¼Œæå–å…¬å…± OSAL ä»»åŠ¡åˆå§‹åŒ–å’Œ IRQ é”å®ã€‚
- **å½±å“æ–‡ä»¶**ï¼š
  - `ws63_final_osal.h/c`ï¼šæ–°å¢ `ws63_task_create_with_queue` å®åŠ `WS63_FINAL_IRQ_LOCK/UNLOCK`ã€‚
  - `ws63_final_task_camera.c`, `ws63_final_task_device_ctrl.c`, `ws63_final_task_lock_mgr.c`, `ws63_final_task_rgb.c`ï¼šæ›¿æ¢ä»»åŠ¡åˆ›å»ºé‡å¤ä»£ç ã€‚
  - æ‰€æœ‰ `Task` ä¸‹çš„ C æ–‡ä»¶ï¼šæ›¿æ¢ç§æœ‰ IRQ é”ä¸ºç»Ÿä¸€å…¬å…±é”ã€‚
- **éªŒè¯ç»“æœ**ï¼šé€šè¿‡ `python3 build.py ws63-liteos-app` ç¼–è¯‘æµ‹è¯•ã€‚
- **åç»­å¾…åŠäº‹é¡¹**ï¼šæ— ã€‚

## ç»´æŠ¤è®°å½•

### 2026-04-14: LD2402 è·ç¦» < 40mm å¼‚å¸¸å€¼è¿‡æ»¤
- **ä¿®æ”¹æ‘˜è¦**ï¼šé—¨é”åœºæ™¯ä¸­ LDæ£€æµ‹æœ‰æ—¶å€™ä¼šæœ‰bugï¼Œæ£€æµ‹distanceå€¼å°äº40æ—¶å½“ä½œæ€ªå¼‚æ•°æ®ï¼Œç›´æ¥æ‰”æ‰ä¸å“åº”ã€‚å› æ­¤åœ¨è§£ææ–‡æœ¬å’ŒäºŒè¿›åˆ¶å¸§æ—¶å¿½ç•¥ < 40mm çš„è·ç¦»æ•°æ®ã€‚
- **æ¶‰åŠæ–‡ä»¶**ï¼š
  - `src/application/mine/ws63_final/Driver/ld2402.c`
- **éªŒè¯**ï¼šç¼–è¯‘æµ‹è¯•æˆåŠŸã€‚
