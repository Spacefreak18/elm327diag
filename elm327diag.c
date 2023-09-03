/*
 * elm327diag.c
 *
 * ELM327 diagnostics
 *
 * Copyright (C) 2023 Paul Jones
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <asm-generic/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <byteswap.h>

#include "elm327.h"

/* Default values for the options */
#define DEFAULT_DEVICE_NAME "/dev/pts/8"
#define DEFAULT_OUTPUT_FILE      "carstats.csv"

/* Options */
const char* device_name = DEFAULT_DEVICE_NAME;
const char* output_file = DEFAULT_OUTPUT_FILE;


typedef enum
{
    INTEGER    = 0,
    DOUBLE     = 1,
}
DataType;

typedef enum
{
    PERCENT           = 0,
    RPM               = 1,
    CELSIUS           = 2,
    PASCALS           = 3,
    KILOMETERSPERHOUR = 4
}
Units;

struct obdpid
{
    int command;
    double min;
    double max;
    size_t bytes;;
    DataType datatype;
    Units units;
    double (*calculate) (double, double);
    //char* command;
    char* commandname;
};


/* Parse command line arguments */
void parse_args(int argc, char* argv[])
{
    int i;

    int help = (argc < 2);

    for (i=1; i<argc && !help; i++)
    {
        if (!strcmp(argv[i],"-d"))
        {
            if (i<argc-1)
            {
                device_name = argv[++i];
            }
            else
            {
                help = 1;
            }
        }
        else
            if (!strcmp(argv[i],"-f"))
            {
                if (i<argc-1)
                {
                    output_file = argv[++i];
                }
                else
                {
                    help = 1;
                }
            }

    }


    if (help)
    {
        printf("-------- elm327diag - Diagnostics Utility for ELM327 Devices --------\n");
        printf("Description:\n");
        printf("  This program is for interfacing with ELM327 serial devices which can \n");
        printf("  read diagnostic data through a vehicle's ODBII port.\n");
        printf("Usage:\n");
        printf("  %s <option> [<option>...]\n",argv[0]);
        printf("Options:\n");
        printf("  -d <string>  device name (default: %s)\n",DEFAULT_DEVICE_NAME);
        printf("  -f <string>  output file name (default: %s)\n",DEFAULT_OUTPUT_FILE);
        printf("  -o           dummy option (useful because at least one option is needed)\n");
        exit(1);
    }
}


double rpmcalc(double a, double b)
{
    return ((a*256)+b)/4;
}

double stdcalc(double a, double b)
{
    return a;
}

void setupcommands(struct obdpid o[25]) {


//3	03	Fuel system status	31	16	1	0		Encoded

    o[3].datatype = 0;
    o[3].command = 0x03;
    o[3].commandname = "Fuel System Status";
    o[3].bytes = 1;

//4	04	Calculated engine load	31	8	1/2.55	0	0 | 100	%

    o[4].datatype = 0;
    o[4].command = 0x04;
    o[4].commandname = "Calculated Engine Load";
    o[4].min = 0;
    o[4].max = 100;
    o[4].units = PERCENT;
    o[4].bytes = 1;

//5	05	Engine coolant temperature	31	8	1	-40	-40 | 215	degC

    o[5].datatype = 0;
    o[5].command = 0x05;
    o[5].commandname = "Engine Coolant Temperature";
    o[5].min = -40;
    o[5].max = 215;
    o[5].units = CELSIUS;
    o[5].bytes = 1;

//6	06	Short term fuel trim (bank 1)	31	8	1/1.28	-100	-100 | 99	%
//7	07	Long term fuel trim (bank 1)	31	8	1/1.28	-100	-100 | 99	%
//8	08	Short term fuel trim (bank 2)	31	8	1/1.28	-100	-100 | 99	%
//9	09	Long term fuel trim (bank 2)	31	8	1/1.28	-100	-100 | 99	%
//10	0A	Fuel pressure (gauge pressure)	31	8	3	0	0 | 765	kPa

    o[10].datatype = 0;
    o[10].command = 0x0A;
    o[10].commandname = "Fuel Gauge Pressure";
    o[10].min = 0;
    o[10].max = 765;
    o[10].units = PASCALS;
    o[10].bytes = 1;

//11	0B	Intake manifold absolute pressure	31	8	1	0	0 | 255	kPa

    o[11].datatype = 0;
    o[11].command = 0x0B;
    o[11].commandname = "Intake Manifold Absolute Pressure";
    o[11].min = 0;
    o[11].max = 255;
    o[11].units = PASCALS;
    o[11].bytes = 1;

//12	0C	Engine speed	31	16	0.25	0	0 | 16384	rpm

    o[12].datatype = 1;
    o[12].command = 0x0C;
    o[12].commandname = "Engine Speed";
    o[12].min = 0;
    o[12].max = 16383.75;
    o[12].units = RPM;
    o[12].bytes = 2;
    o[12].calculate = rpmcalc;

//13	0D	Vehicle speed	31	8	1	0	0 | 255	km/h

    o[13].datatype = 0;
    o[13].command = 0x0D;
    o[13].commandname = "Vehicle Speed";
    o[13].min = 0;
    o[13].max = 255;
    o[13].units = KILOMETERSPERHOUR;
    o[13].bytes = 1;


//14	0E	Timing advance	31	8	0.5	-64	-64 | 64	deg
//15	0F	Intake air temperature	31	8	1	-40	-40 | 215	degC
//16	10	Mass air flow sensor air flow rate	31	16	0.01	0	0 | 655	grams/sec
//17	11	Throttle position	31	8	1/2.55	0	0 | 100	%
//18	12	Commanded secondary air status	31	8	1	0		Encoded
//19	13	Oxygen sensors present (2 banks)
//20	14	Oxygen sensor 1 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 1 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//21	15	Oxygen sensor 2 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 2 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//22	16	Oxygen sensor 3 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 3 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//23	17	Oxygen sensor 4 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 4 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//24	18	Oxygen sensor 5 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 5 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//25	19	Oxygen sensor 6 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 6 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//26	1A	Oxygen sensor 7 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 7 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//27	1B	Oxygen sensor 8 (voltage)	31	8	0.005	0	0 | 1	volts
//Oxygen sensor 9 (short term fuel trim)	39	8	1/1.28	-100	-100 | 99	%
//28	1C	OBD standards the vehicle conforms to	31	8	1	0		Encoded
//29	1D	Oxygen sensors present (4 banks)
//30	1E	Auxiliary input status
//31	1F	Run time since engine start	31	16	1	0	0 | 65535	seconds
//32	20	PIDs supported [21 - 40]	31	32	1	0		Encoded
//33	21	Distance traveled with MIL on	31	16	1	0	0 | 65535	km
//34	22	Fuel rail pres. (rel. to manifold vacuum)	31	16	0.079	0	0 | 5177	kPa
//35	23	Fuel rail gauge pres. (diesel, gas inject)	31	16	10	0	0 | 655350	kPa
//36	24	Oxygen sensor 1 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 1 (voltage)	47	16	1/8192	0	0 | 2	volts
//37	25	Oxygen sensor 2 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 2 (voltage)	47	16	1/8192	0	0 | 8	volts
//38	26	Oxygen sensor 3 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 3 (voltage)	47	16	1/8192	0	0 | 8	volts
//39	27	Oxygen sensor 4 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 4 (voltage)	47	16	1/8192	0	0 | 8	volts
//40	28	Oxygen sensor 5 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 5 (voltage)	47	16	1/8192	0	0 | 8	volts
//41	29	Oxygen sensor 6 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 6 (voltage)	47	16	1/8192	0	0 | 8	volts
//42	2A	Oxygen sensor 7 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 7 (voltage)	47	16	1/8192	0	0 | 8	volts
//43	2B	Oxygen sensor 8 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 8 (voltage)	47	16	1/8192	0	0 | 8	volts
//44	2C	Commanded EGR	31	8	1/2.55	0	0 | 100	%
//45	2D	EGR Error	31	8	1/1.28	-100	-100 | 99	%
//46	2E	Commanded evaporative purge	31	8	1/2.55	0	0 | 100	%
//47	2F	Fuel tank level input	31	8	1/2.55	0	0 | 100	%
//48	30	Warmups since DTCs cleared	31	8	1	0	0 | 255	count
//49	31	Distance traveled since DTCs cleared	31	16	1	0	0 | 65535	km
//50	32	Evap. system vapor pressure	31	16	0.25	0	-8192 | 8192	Pa
//51	33	Absolute barometric pressure	31	8	1	0	0 | 255	kPa
//52	34	Oxygen sensor 1 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 1 (current)	47	16	1/256	-128	-128 | 128	mA
//53	35	Oxygen sensor 2 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 2 (current)	47	16	1/256	-128	-128 | 128	mA
//54	36	Oxygen sensor 3 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 3 (current)	47	16	1/256	-128	-128 | 128	mA
//55	37	Oxygen sensor 4 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 4 (current)	47	16	1/256	-128	-128 | 128	mA
//56	38	Oxygen sensor 5 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 5 (current)	47	16	1/256	-128	-128 | 128	mA
//57	39	Oxygen sensor 6 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 6 (current)	47	16	1/256	-128	-128 | 128	mA
//58	3A	Oxygen sensor 7 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 7 (current)	47	16	1/256	-128	-128 | 128	mA
//59	3B	Oxygen sensor 8 (air-fuel equiv. ratio)	31	16	1/32768	0	0 | 2	ratio
//Oxygen sensor 8 (current)	47	16	1/256	-128	-128 | 128	mA
//60	3C	Catalyst temperature (bank 1, sensor 1)	31	16	0.1	-40	-40 | 6514	degC
//61	3D	Catalyst temperature (bank 2, sensor 1)	31	16	0.1	-40	-40 | 6514	degC
//62	3E	Catalyst temperature (bank 1, sensor 2)	31	16	0.1	-40	-40 | 6514	degC
//63	3F	Catalyst temperature (bank 2, sensor 2)	31	16	0.1	-40	-40 | 6514	degC
//64	40	PIDs supported [41 - 60]	31	32	1	0		Encoded
//65	41	Monitor status this drive cycle	31	32	1	0		Encoded
//66	42	Control module voltage	31	16	0.001	0	0 | 66	V
//67	43	Absolute load value	31	16	1/2.55	0	0 | 25700	%
//68	44	Commanded air-fuel equiv. ratio	31	16	1/32768	0	0 | 2	ratio
//69	45	Relative throttle position	31	8	1/2.55	0	0 | 100	%
//70	46	Ambient air temperature	31	8	1	-40	-40 | 215	degC
//71	47	Absolute throttle position B	31	8	1/2.55	0	0 | 100	%
//72	48	Absolute throttle position C	31	8	1/2.55	0	0 | 100	%
//73	49	Accelerator pedal position D	31	8	1/2.55	0	0 | 100	%
//74	4A	Accelerator pedal position E	31	8	1/2.55	0	0 | 100	%
//75	4B	Accelerator pedal position F	31	8	1/2.55	0	0 | 100	%
//76	4C	Commanded throttle actuator	31	8	1/2.55	0	0 | 100	%
//77	4D	Time run with MIL on	31	16	1	0	0 | 65535	minutes
//78	4E	Time since DTCs cleared	31	16	1	0	0 | 65535	minutes
//79	4F	Max fuel-air equiv. ratio	31	8	1	0	0 | 255	ratio
//Max oxygen sensor voltage	39	8	1	0	0 | 255	V
//Max oxygen sensor current	47	8	1	0	0 | 255	mA
//Max intake manifold absolute pressure	55	8	10	0	0 | 2550	kPa
//80	50	Max air flow rate from MAF sensor	31	8	10	0	0 | 2550	g/s
//81	51	Fuel type	31	8	1	0		Encoded
//82	52	Ethanol fuel percentage	31	8	1/2.55	0	0 | 100	%
//83	53	Absolute evap system vapor pressure	31	16	0.005	0	0 | 328	kPa
//84	54	Evap system vapor pressure	31	16	1	-32767	-32767 | 32768	Pa
//85	55	Short term sec. oxygen trim (bank 1)	31	8	1/1.28	-100	-100 | 99	%
//Short term sec. oxygen trim (bank 3)	39	8	1/1.28	-100	-100 | 99	%
//86	56	Long term sec. oxygen trim (bank 1)	31	8	1/1.28	-100	-100 | 99	%
//Long term sec. oxygen trim (bank 3)	39	8	1/1.28	-100	-100 | 99	%
//87	57	Short term sec. oxygen trim (bank 2)	31	8	1/1.28	-100	-100 | 99	%
//Short term sec. oxygen trim (bank 4)	39	8	1/1.28	-100	-100 | 99	%
//88	58	Long term sec. oxygen trim (bank 2)	31	8	1/1.28	-100	-100 | 99	%
//Long term sec. oxygen trim (bank 4)	39	8	1/1.28	-100	-100 | 99	%
//89	59	Fuel rail absolute pressure	31	16	10	0	0 | 655350	kPa
//90	5A	Relative accelerator pedal position	31	8	1/2.55	0	0 | 100	%
//91	5B	Hybrid battery pack remaining life	31	8	1/2.55	0	0 | 100	%
//92	5C	Engine oil temperature	31	8	1	-40	-40 | 215	degC
//93	5D	Fuel injection timing	31	16	1/128	-210	-210 | 302	deg
//94	5E	Engine fuel rate	31	16	0.05	0	0 | 3277	L/h
//95	5F	Emission requirements	31	8	1	0		Encoded
//96	60	PIDs supported [61 - 80]	31	32	1	0		Encoded
//97	61	Demanded engine percent torque	31	8	1	-125	-125 | 130	%
//98	62	Actual engine percent torque	31	8	1	-125	-125 | 130	%
//99	63	Engine reference torque	31	16	1	0	0 | 65535	Nm
//100	64	Engine pct. torque (idle)	31	8	1	-125	-125 | 130	%
//Engine pct. torque (engine point 1)	39	8	1	-125	-125 | 130	%
//Engine pct. torque (engine point 2)	47	8	1	-125	-125 | 130	%
//Engine pct. torque (engine point 3)	55	8	1	-125	-125 | 130	%
//Engine pct. torque (engine point 4)	63	8	1	-125	-125 | 130	%
//101	65	Auxiliary input/output supported	31	8	1	0		Encoded
//102	66	Mass air flow sensor (A)	39	16	1/32	0	0 | 2048	grams/sec
//Mass air flow sensor (B)	55	16	1/32	0	0 | 2048	grams/sec
//103	67	Engine coolant temperature (sensor 1)	39	8	1	-40	-40 | 215	degC
//Engine coolant temperature (sensor 2)	47	8	1	-40	-40 | 215	degC
//104	68	Intake air temperature (sensor 1)	39	8	1	-40	-40 | 215	degC
//Intake air temperature (sensor 2)	47	8	1	-40	-40 | 215	degC
//105	69	Commanded EGR and EGR error
//106	6A	Com. diesel intake air flow ctr/position
//107	6B	Exhaust gas recirculation temperature
//108	6C	Com. throttle actuator ctr./position
//109	6D	Fuel pressure control system
//110	6E	Injection pressure control system
//111	6F	Turbocharger compressor inlet pres.
//112	70	Boost pressure control
//113	71	Variable geometry turbo control
//114	72	Wastegate control
//115	73	Exhaust pressure
//116	74	Turbocharger RPM
//117	75	Turbocharger temperature
//118	76	Turbocharger temperature
//119	77	Charge air cooler temperature
//120	78	EGT (bank 1)
//121	79	EGT (bank 2)
//122	7A	Diesel particulate filter - diff. pressure
//123	7B	Diesel particulate filter
//124	7C	Diesel particulate filter - temperature	31	16	0.1	-40	-40 | 6514	degC
//125	7D	NOx NTE control area status
//126	7E	PM NTE control area status
//127	7F	Engine run time						seconds
//128	80	PIDs supported [81 - A0]	31	32	1	0		Encoded
//129	81	Engine run time for AECD
//130	82	Engine run time for AECD
//131	83	NOx sensor
//132	84	Manifold surface temperature
//133	85	NOx reagent system
//134	86	Particulate matter sensor
//135	87	Intake manifold absolute pressure
//136	88	SCR induce system
//137	89	Run time for AECD #11-#15
//138	8A	Run time for AECD #16-#20
//139	8B	Diesel aftertreatment
//140	8C	O2 sensor (wide range)
//141	8D	Throttle position G	31	8	1/2.55	0	0 | 100	%
//142	8E	Engine friction percent torque	31	8	1	-125	-125 | 130	%
//143	8F	Particulate matter sensor (bank 1 & 2)
//144	90	WWH-OBD vehicle OBD system Info						hours
//145	91	WWH-OBD vehicle OBD system Info						hours
//146	92	Fuel system control
//147	93	WWH-OBD counters support						hours
//148	94	NOx warning and inducement system
//152	98	EGT sensor
//153	99	EGT sensor
//154	9A	Hybrid/EV sys. data, battery, voltage
//155	9B	Diesel exhaust fluid sensor data
//156	9C	O2 sensor data
//157	9D	Engine fuel rate						g/s
//158	9E	Engine exhaust flow rate						kg/h
//159	9F	Fuel system percentage use
//160	A0	PIDs supported [A1 - C0]	31	32	1	0		Encoded
//161	A1	NOx sensor corrected data						ppm
//162	A2	Cylinder fuel rate	31	16	1/32	0	0 | 2048	mg/stroke
//163	A3	Evap system vapor pressure
//164	A4	Transmission actual gear	47	16	0.001	0	0 | 66	ratio
//165	A5	Cmd. diesel exhaust fluid dosing	39	8	0.5	0	0 | 128	%
//166	A6	Odometer	31	32	0.1	0	0 | 429496730	km
//167	A7	NOx concentration 3, 4
//168	A8	NOx corrected concentration (3, 4)
//192	C0	PIDs supported [C1 - E0]	31	32	1	0		Encoded







}

int query_elm(
    int            elm327_mod_fd,
    OBD_MODE       mode,
    OBD_PARAM      pid,
    elm327_msg_t **msgs,   /* Returned data from ELM327                 */
    int           *n_msgs, /* Number of messages returned               */
    int            ascii)  /* True if we want ascii vs binary data back */
{
    elm327_msg_t send_msg;

    elm327_create_msg(send_msg, mode, pid);

    /* Send */
    if (elm327_send_msg(elm327_mod_fd, send_msg) == -1)
      return 1;

    /* Receive */
    if ((*msgs = elm327_recv_msgs(elm327_mod_fd, n_msgs, 0)) == NULL)
      return 2;

    /* Flush */
    elm327_flush(elm327_mod_fd);

    return 0;
}

#define QUERY_OR_ERR(_fd, _mode, _pid, _recv, _n_recv, _ascii)           \
{                                                                   \
    int _err;                                               \
                                                                    \
    if ((_err = query_elm(                                          \
            _fd, _mode, _pid, _recv, _n_recv, _ascii)) != 0) \
    {                                                               \
        elm327_destroy_recv_msgs(*_recv);                           \
        return _err;                                                \
    }                                                               \
}


int main(int argc, char* argv[])
{
    parse_args(argc,argv);

    /* Open the device */
    fprintf(stdout, "initializing connection\n");
    int elm_fd = elm327_init(device_name);

    int timeout = 3000;
    elm327_set_timeout(timeout);

    fprintf(stdout, "initializing vehicle info pids\n");
    struct obdpid o[25];
    for (int i = 0; i < 25; i++)
    {
        o[i].bytes = 0;
        o[i].calculate = stdcalc;
    }
    setupcommands(o);


    // TODO: Ensure and put device into known good state

    //elm327_msg_t *recv_msg = NULL;
    //QUERY_OR_ERR(elm_fd, OBD_MODE_1, 0x0D, &recv_msg, NULL, 0);
    //double b = (double)((*recv_msg)[2]);
    //elm327_destroy_recv_msgs(recv_msg);

    {

        fprintf(stdout, "gathering data...\n");
        FILE *out = fopen(output_file, "w");
        for(int j = 0; j < 25; j++)
        {
            if (o[j].bytes > 0)
            {

                elm327_msg_t *recv_msg = NULL;
                QUERY_OR_ERR(elm_fd, OBD_MODE_1, o[j].command, &recv_msg, NULL, 0);

                double b1 = (double)((*recv_msg)[2]);
                double b2 = (double)((*recv_msg)[3]);
                double r = o[j].calculate(b1, b2);

                elm327_destroy_recv_msgs(recv_msg);

                fprintf(out, "%s, %f\n", o[j].commandname, r);
            }
        }


        fprintf(stdout, "done\n");
        fclose(out);

    }

    elm327_shutdown(elm_fd);

}
