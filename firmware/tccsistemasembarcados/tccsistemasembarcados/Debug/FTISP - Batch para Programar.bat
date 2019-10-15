:: Utiliza o conversor do winavr para converter de intel Hex para binario.

avr-objcopy -I ihex "TccSistemasEmbarcados.hex" -O binary temp.bin

pause

:: Utiliza o proframador isp da ftdi para gravar o programa no microcontrolador atravez da isp pela usb.O processador é identificado automaticamente atravez dos bits de assinatura, - B seta o clock do ftdi para 100khz, -E apaga o processador, -fw grava o programa na flash, -Fw grava os fuse bits (3 bytes em hexa respectivamente com msb= extended, high, lsb low), -fv verifica o que está na flash e compara com o programa no pc, -Fr le os fuses e -Lr le os lock bits.

ftisp.exe -B 100000 -E -fw temp.bin -Fw 0xFFDCFF -fv temp.bin -Fr -Lr

pause