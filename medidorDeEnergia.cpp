#include "mbed.h"
#include <cstdint>
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"

#define currIn p20 //Pin de Corriente
#define voltIn p17 //Pin de Voltaje
#define SDA p28 //Pin SDA de la ssd1306
#define SCL p27 //Pin SDA de la ssd1306
#define size 2 //Definimos el tamaño de la letra en la pantalla
#define color 1 //Se define el color de la letra de la ssd
#define N 100  //numero de muestras a tomar

//Creacion de instancias de a clase ADC para los sensores de entrada
AnalogIn adc_i(currIn), adc_v(voltIn);
DigitalOut led1(LED1);
Ticker muestreo_interrupt; 

uint8_t ready_flag = 0; //Bandera
float carr[N];
float varr[N];
volatile uint16_t i_vect[N], v_vect[N], idx_arr[4];; 
volatile float i_vectR[N], v_vectR[N]; 

class I2CPreInit : public I2C {
public:
    I2CPreInit(PinName sda, PinName scl) : I2C(sda, scl){
        frequency(400000);
        start();
    };
};
//Una vez definida la clase llamamos creamos un objeto de ella llamada gI2C con los pines previamnte establecidos 
I2CPreInit gI2C(SDA, SCL);
//Ya que la clase este creada la añadimos al la creacion del objeto Adafruit_SSD1306_I2c para mandarle datos a la pantalla
Adafruit_SSD1306_I2c gOled2(gI2C, p26, 0x78, 64, 128);

void mostrarCadena(const char *str){ //Funcion para imprimir cadenas de caracteres
    //Acomodo la ubicacion del cursor
    gOled2.setTextCursor(0, 0); 
    //Acomodo el color de la letra
    gOled2.setTextColor(color);
    //Acomoda el tamaño de la letra
    gOled2.setTextSize(size); 
    //Recorre todos los valores de la cadena hasta que un caracter sea nulo
    while (*str != '\0'){ 
        gOled2.writeChar(*str);
        str++; //Contador de caracteres
    }
    //Actualiza el display
    gOled2.display(); 
}

//Inicializa el display 
void initalizeDisplay(){
    //Inicializa las librerias
    gOled2.begin(); 
    //Limpia el display de cualquier cosa que tenga escrita
    gOled2.clearDisplay(); 
    //Setea el cursor de escritura en 0,0 
    gOled2.setTextCursor(0,0);
}

void muestreo(){
    static uint16_t i=0; //contador de muestreos tomadas en este periodo
    if(i < N){
        i_vect[i] = adc_i.read_u16();
        //v_vect[i] = (adc_v.read_u16()*0.0049); //llenado de vector de voltaje
        v_vect[i] = adc_v.read_u16();
        //printf("Volatage: %4.3hu V, Corriente: %4.3hu A",adc_v.read_u16(),adc_i.read_u16());
        i++;
    }else{
        if(ready_flag == 0){
            i=0;
        }
        ready_flag = 1;
    }
}

void secondOrderFilter(volatile uint16_t array[], volatile float result[]) {
    // Coeficientes del filtro (Filtro de segundo orden Butterworth)
    float b1 = 0.007442, b2 = 0.007013;
    float a1 = 1.83, a2 = - 0.837;
    // Aplicar el filtro
    result[0] = array[0];
    result[1] = array[1];
    for (int k = 2; k < N; k++) {
        result[k] = b1 * array[k - 1] + b2 * array[k - 2] - a1 * result[k - 1] - a2 * result[k - 2];
    }
}

void funcionPico(volatile uint16_t* arrayA, volatile uint16_t* arrayB,volatile uint16_t* idx_arr) {
    //Se inicialisan las variables de picos i vall  es conn el primer valor del array y de ahi comparar el resto
    float pk_val1 = arrayA[0], pk_val2 = arrayB[0], vall1 = arrayA[0], vall2 = arrayB[0];
    //Iteramos sobre los arrays para detectar cual de los valores almacenados es el mayor y el menor
    for (int i = 1; i < N; i++) {
        if (arrayA[i] > pk_val1) {
            pk_val1 = arrayA[i];
            idx_arr[0] = pk_val1;
        }else if (arrayA[i] < vall1) {
            vall1 = arrayA[i];
            idx_arr[2] = vall1;
        }
        if (arrayB[i] > pk_val2) {
            pk_val2 = arrayB[i];
            idx_arr[1] = pk_val2;
        }else if (arrayB[i] < vall2) {
            vall2 = arrayB[i];
            idx_arr[3] = vall2;
        }
    }
}

void desarrollo(){
    uint16_t idx_arr[4];
    char string[20], string2[20], string3[20], string4[20], string5[20], string6[20], string7[20], string8[20];
    //Hacemos un arreglo de cadenas para que estas puedan ser iterables, y por consiguiente poder volver todos los valores que se van a 
    //hallar en strings sin tener que llamar la misma funcion varias veces desde este proceso
    char *cadenas[] = {string, string2, string3, string4, string5, string6, string7, string8}; 
    //En los arrays de resultado va ir la concatenacion de todos los resultados obtenidos previamnte 
    char resultado1[100], resultado2[100];
    //Aplicamos el filtro de segundo orden para la corriente
    secondOrderFilter(i_vect, i_vectR);
    //Aplicamos el filtro de segundo orden para el voltage
    secondOrderFilter(v_vect, v_vectR);
    //Hallamos los valores picos para la señal filtrada
    funcionPico(i_vect, v_vect, idx_arr);
    printf("Corriente P %hu,Voltage Pico %hu\n",idx_arr[0],idx_arr[1]);
    //Definimos las variales
    float i_pk = 0, v_pk = 0, vrms = 0, irms = 0, angulo = 0, s = 0, fp = 0, Q = 0, P = 0;
    if(idx_arr[0] > 49800){
        //Hallamos los valores pico para la corriente y el voltage
        i_pk = idx_arr[0] * 0.0000192; v_pk = idx_arr[1] * 0.002215;
        //Despues hallamos los valores rms de de la corriente y el voltage 
        vrms = v_pk/sqrt(2); irms=i_pk/sqrt(2);
        //Angulo formado por la potencia activa y la potencia aparente.
        angulo = ((idx_arr[0]-idx_arr[1])*(1.0/60.0*N)*377.0);
        //Potencia aparente // Factor de potencia
        s = vrms*irms;         fp = cos(angulo);
        // Potencia Reactiva // Potencia Activa
        Q = s*sin(angulo);      P = s*fp;
    }
    //Los valores hallados los ingresamos a un vector para poder iterarlos y convertirlos en strings
    float valores[] = {i_pk, v_pk, irms, vrms, s, fp, Q, P};
    //Mostramos los resultados previamente hallados por consola
    printf("i_pk = %4.3fA, v_pk = %4.3fV\n", i_pk, v_pk);
    printf("Irms = %4.3fA, Vrms= %4.3fV\n", irms, vrms);
    printf("Aparente = %4.3fVA, F.potencia= %f\n", s, fp);
    printf("P.reactiva = %4.3fVAR, P.activa= %fW\n", Q, P);
    //Mostramos los valores de las muestras que se tomaron tanto para corriente como para voltage 
    for(int i=0; i<N; i++){
        //printf("%f, %f \n", i_vectR[i], v_vectR[i]);
    }
    //Convertimos cada valor flotante en String en el siguiente for 
    for (int i = 0; i < sizeof(valores)/sizeof(valores[0]); i++) {
        snprintf(cadenas[i], sizeof(string), "%.2f", valores[i]);
    }
    //Incicialisamos el display
    initalizeDisplay();
    //Una vez tenemos los datos concatenados y convertidos en strings podemos juntarlos todos para mostrar el resultado
    snprintf(resultado1, sizeof(resultado1), "Ip=%s\nVp=%s\nIr=%s\nVr=%s", string, string2, string3, string4);
    snprintf(resultado2, sizeof(resultado2), "S=%s\nFP=%s\nQ=%s\nP=%s", string5,string6,string7,string8);
    //Gracias a que se nesesita mostrar una gran cantidad de datos y la pantalla no es muy grande para que se vean todos se 
    //divide en resultado 1 y 2, y primero mostramos el 1
    mostrarCadena(resultado1);
    //Despues esperamos 2 segundos para que de el tiempo para leer los primeros resultados para luego proceder con el resultado 2
    thread_sleep_for(1500); gOled2.clearDisplay(); 
    //Mostramos el resultado 2
    mostrarCadena(resultado2);
    //Damos tiempo para que se alcacen a ver los resultados 
    thread_sleep_for(1500);
}

int main() {
    printf("This is a test for the ssd1306\n");
    muestreo_interrupt.attach(&muestreo,167us);
    while (true) {
        thread_sleep_for(100);
        if(ready_flag == 1){
            funcionPico(i_vect, v_vect, idx_arr);
            printf("Corriente Pico: %hu, Voltage Pico: %hu\n",idx_arr[0],idx_arr[1]);
            muestreo_interrupt.detach();
            desarrollo();
            ready_flag = 0;
            //Llamamos a la funcion muestreo cada 1/(60.0*N) milisegundos
            muestreo_interrupt.attach(&muestreo,167us);
        }
    }
}
