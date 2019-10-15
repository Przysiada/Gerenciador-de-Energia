/*
 * TccSistemasEmbarcados.c
 *
 * Created: 19/09/2015 13:22:09
 *  Author: Diego Merks
 */ 

#define F_CPU 11059200

#define CLR(port,pin)	PORT ## port &= ~(1<<pin)
#define SET(port,pin)	PORT ## port |=  (1<<pin)
#define TOGL(port,pin)	PORT ## port ^=  (1<<pin)
#define READ(port,pin)	PIN  ## port &   (1<<pin)
#define OUT(port,pin)	DDR  ## port |=  (1<<pin)
#define IN(port,pin)	DDR  ## port &= ~(1<<pin)

#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <math.h>

typedef enum ExpectedResponse {
	RESPONSE_OK = 0, RESPONSE_DATA_INPUT_MODE = 1, RESPONSE_NONE = 2
} ExpectedResponse;

void ADCInit(void);
void ADCStartConversion(void);
void USARTInit(uint16_t ubrr_value);
uint8_t USARTReadChar(void);
void USARTWriteChar(uint8_t data);
double calculateConsumption(void);
void waitForModuleResponseReady(void);
void waitForModuleResponseOk(void);
void waitForModuleResponseDataInputMode(void);
void sendAtCommand(uint8_t* command, uint8_t commandLength, ExpectedResponse expectedResponse);
void ESP12Init(void);
void sendDataToServer(void);
uint8_t receiveDataFromServer(void);

uint8_t ATCWQAP[] = "AT+CWQAP\r\n";
uint8_t ATCWQAPLength = 10;
uint8_t ATCWJAP[] = "AT+CWJAP=\"Merks\",\"batatafrita\"\r\n";
uint8_t ATCWJAPLength = 32;
uint8_t ATCIPSTART[] = "AT+CIPSTART=\"TCP\",\"10.0.0.100\",12345\r\n";
uint8_t ATCIPSTARTLength = 38;
uint8_t ATCIPSEND[] = "AT+CIPSEND=0,2\r\n";
uint8_t ATCIPSENDLength = 16;
uint8_t ATCWSAP[] = "AT+CWSAP=\"Medidor\",\"embarcados\",5,3\r\n";
uint8_t ATCWSAPLength = 37;
uint8_t ATCWMODE[] = "AT+CWMODE=2\r\n";
uint8_t ATCWMODELength = 13;
uint8_t ATCIPSERVER[] = "AT+CIPSERVER=1,12345\r\n";
uint8_t ATCIPSERVERLength = 22;
uint8_t ATCIPMUX[] = "AT+CIPMUX=1\r\n";
uint8_t ATCIPMUXLength = 13;

double instantADCVoltage[80];
uint8_t instantADCVoltageIndex;
uint8_t adcInterruptCounter;

uint8_t consumptionH, consumptionL;

uint8_t byte1 = 0, byte2 = 0, byte3 = 0, byte4 = 0, byte5 = 0, byte6 = 0, byte7 = 0, byte8 = 0, byte9 = 0, byte10 = 0;

ISR(USART_RX_vect)
{
	byte1 = byte2;
	byte2 = byte3;
	byte3 = byte4;
	byte4 = byte5;
	byte5 = byte6;
	byte6 = byte7;
	byte7 = byte8;
	byte8 = byte9;
	byte9 = byte10;
	byte10 = UDR0;

	if(byte1 == '+' && byte2 == 'I' && byte3 == 'P' && byte4 == 'D' && byte5 == ',' && byte6 == '0' && byte7 == ',' && byte8 == '1' && byte9 == ':') {
		if(byte10 == 1) SET(B, 1);
		else if(byte10 == 0) CLR(B, 1);
	}
	
	if(byte10 == '>') {
		USARTWriteChar(consumptionH);
		USARTWriteChar(consumptionL);
	}
}

ISR(ADC_vect) {
	
	if(++adcInterruptCounter >= 72) {
		
		instantADCVoltage[instantADCVoltageIndex] = (ADCW * 0.0048828125f);
		
		if(instantADCVoltageIndex++ >= 80)
			instantADCVoltageIndex = 0;
			
		adcInterruptCounter = 0;
	}
	
	ADCStartConversion();
}

void waitForModuleResponseReady(void) {
	uint8_t byte1 = 0, byte2 = 0, byte3 = 0, byte4 = 0, byte5 = 0, byte6 = 0, byte7 = 0;
	
	while(byte1 != 'r' || byte2 != 'e' || byte3 != 'a' || byte4 != 'd' || byte5 != 'y' || byte6 != 13 || byte7 != 10) {
		byte1 = byte2;
		byte2 = byte3;
		byte3 = byte4;
		byte4 = byte5;
		byte5 = byte6;
		byte6 = byte7;
		byte7 = USARTReadChar();
	}
}

void waitForModuleResponseOk(void) {
	uint8_t byte1 = 0, byte2 = 0, byte3 = 0, byte4 = 0;

	while(byte1 != 'O' || byte2 != 'K' || byte3 != 13 || byte4 != 10) {
		byte1 = byte2;
		byte2 = byte3;
		byte3 = byte4;
		byte4 = USARTReadChar();
	}
}

void waitForModuleResponseDataInputMode(void) {
	uint8_t byte1 = 0, byte2 = 0;

	while(byte1 != '>' || byte2 != ' ') {
		byte1 = byte2;
		byte2 = USARTReadChar();
	}
}

void sendAtCommand(uint8_t* command, uint8_t commandLength, ExpectedResponse expectedResponse) {
	uint8_t i;
	
	for(i = 0; i < commandLength; i++) USARTWriteChar(command[i]);
	
	if(expectedResponse == RESPONSE_OK) waitForModuleResponseOk();
	else if(expectedResponse == RESPONSE_DATA_INPUT_MODE) waitForModuleResponseDataInputMode();
}

void ESP12Init(void) {
	waitForModuleResponseReady();
	sendAtCommand(ATCWQAP, ATCWQAPLength, RESPONSE_OK);
	sendAtCommand(ATCWMODE, ATCWMODELength, RESPONSE_OK);
	sendAtCommand(ATCWSAP, ATCWSAPLength, RESPONSE_OK);
	sendAtCommand(ATCIPMUX, ATCIPMUXLength, RESPONSE_OK);
	sendAtCommand(ATCIPSERVER, ATCIPSERVERLength, RESPONSE_OK);	
	//sendAtCommand(ATCWJAP, ATCWJAPLength, RESPONSE_OK);
	//sendAtCommand(ATCIPSTART, ATCIPSTARTLength, RESPONSE_OK);
}

void sendDataToServer(void) {
	sendAtCommand(ATCIPSEND, ATCIPSENDLength, RESPONSE_NONE);
}

uint8_t receiveDataFromServer(void) {
	uint8_t byte1 = 0, byte2 = 0, byte3 = 0, byte4 = 0, byte5 = 0, byte6 = 0, byte7 = 0, byte8 = 0, byte9 = 0, byte10 = 0;

	while(byte1 != '+' || byte2 != 'I' || byte3 != 'P' || byte4 != 'D' || byte5 != ',' || byte6 != '1' || byte7 != ':' || byte9 != 13 || byte10 != 10) {
		byte1 = byte2;
		byte2 = byte3;
		byte3 = byte4;
		byte4 = byte5;
		byte5 = byte6;
		byte6 = byte7;
		byte7 = byte8;
		byte8 = byte9;
		byte9 = byte10;
		byte10 = USARTReadChar();
	}

	return byte8;
}

double calculateConsumption(void) {
	
	double calculatedConsumption = 0;
	double instantConsumption = 0;
	
	uint8_t i;	
	for(i = 0; i < 80; i++) {
		
		instantConsumption = instantADCVoltage[i] - 2.5f;
		instantConsumption *= 10000;
		
		calculatedConsumption += pow(instantConsumption, 2);
	}
	
	calculatedConsumption /= 80;
	
	return sqrt(calculatedConsumption);
}

void ADCInit(void) {
	
	// Seleciona o canal 0 do ADC, com saída ajustada à direita e tensão de referência definida pelo pino AREF
	ADMUX |= (1 << REFS0);
	
	// Seta o prescaler para 128
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	
	// Desabilita a função digital do pino PC0
	DIDR0 |= (1 << ADC0D);
	
	// Habilita a interrupção do ADC
	ADCSRA = (1 << ADIE);
	
	// Habilita o ADC
	ADCSRA |= (1 << ADEN);
}

void ADCStartConversion(void) {
	
	// Inicia a conversão
	ADCSRA |= (1 << ADSC);
}

void USARTInit(uint16_t ubrr_value) {

	// Configura o baud rate
	UBRR0L = ubrr_value;
	UBRR0H = (ubrr_value >> 8);
	
	// Configura o formato do frame (assíncrono, sem paridade, 1 stop bit e comprimento de 8 bits)
	UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);
	
	// Habilita o transmissor, o receptor e as interrupções de serial
	UCSR0B |= (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
}

uint8_t USARTReadChar(void) {
	
	// Aguarda a chegada de novos dados
	while(!(UCSR0A & (1 << RXC0)));
	
	// Retorna o dado recebido
	return UDR0;
}

void USARTWriteChar(uint8_t data) {
	
	// Aguarda disponibilidade do transmissor
	while(!(UCSR0A & (1 << UDRE0)));
	
	// Escreve o dado no buffer de envio
	UDR0 = data;
}

int main(void)
{
	double calculatedConsumption[3];
	double consumptionAverage;
	uint8_t i;
	uint16_t consumption;
	
	OUT(B, 0);
	OUT(B, 1);
	
	adcInterruptCounter = 0;
	instantADCVoltageIndex = 0;
	
	USARTInit(0x0005);		
	SET(B, 0);
	
	ADCInit();
	ESP12Init();
	sei();
	
	ADCStartConversion();
	for(;;)
	{
		calculatedConsumption[0] = calculatedConsumption[1];
		calculatedConsumption[1] = calculatedConsumption[2];
		calculatedConsumption[2] = calculateConsumption();
		
		consumptionAverage = 0.0f;
		for(i = 0; i < 3; i++){
			consumptionAverage += calculatedConsumption[i];
		}
		
		consumption = (uint16_t)(consumptionAverage / 3);
		
		consumptionH = consumption >> 8;
		consumptionL = consumption;
		
		sendDataToServer();
	}
	
	return 0;
}