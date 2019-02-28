/*
*   file nist.c
*   author Measurement Computing Corp.
*   brief This file contains NIST thermocouple linearization functions.
*
*   date 1 Feb 2018
*/
#include <math.h>
#include "nist.h"

// NIST Thermocouple coefficients
//
// The following types are supported:
//
//	J, K, R, S, T, N, E, B
enum tcTypes
{
    TC_TYPE_J = 0,
    TC_TYPE_K,
    TC_TYPE_T,
    TC_TYPE_E,
    TC_TYPE_R,
    TC_TYPE_S,
    TC_TYPE_B,
    TC_TYPE_N
};


typedef struct 
{
	unsigned int nCoefficients;
	double VThreshold;
	const double* Coefficients;
} NIST_Table_type;

typedef struct
{
	unsigned int nCoefficients;
	const double* Coefficients;
} NIST_Reverse_type;
	
typedef struct
{
	unsigned int nTables;
	const NIST_Reverse_type* ReverseTable;
	const NIST_Table_type* Tables;
} Thermocouple_Data_type;

// ****************************************************************************
// Type J data

const double TypeJTable0[] =
{
	 0.0000000E+00,
	 1.9528268E+01,
	-1.2286185E+00,
	-1.0752178E+00,
	-5.9086933E-01,
	-1.7256713E-01,
	-2.8131513E-02,
	-2.3963370E-03,
	-8.3823321E-05
};

const double TypeJTable1[] =
{	
	 0.000000E+00,	
	 1.978425E+01,	
	-2.001204E-01,	
	 1.036969E-02,	
	-2.549687E-04,	
	 3.585153E-06,	
	-5.344285E-08,	
	 5.099890E-10
};

const double TypeJTable2[] =
{
	-3.11358187E+03,
	 3.00543684E+02, 		
	-9.94773230E+00, 		
	 1.70276630E-01, 		
	-1.43033468E-03, 		
	 4.73886084E-06		
};

const NIST_Table_type TypeJTables[] =
{
	{
		9,
		0.0,
		TypeJTable0
	},
	{
		8,
		42.919,
		TypeJTable1
 	},
 	{
		6,
		69.553, 
		TypeJTable2		
	}
};

const double TypeJReverse[] =
{					
	 0.000000000000E+00,
	 0.503811878150E-01,
	 0.304758369300E-04,
	-0.856810657200E-07,
	 0.132281952950E-09,
	-0.170529583370E-12,
	 0.209480906970E-15,
	-0.125383953360E-18,
	 0.156317256970E-22
};

const NIST_Reverse_type TypeJReverseTable = 
{
	9,						// nCoefficients
	TypeJReverse
};

// ****************************************************************************
// Type K data
const double TypeKTable0[] =
{
	 0.0000000E+00,
	 2.5173462E+01,
	-1.1662878E+00,
	-1.0833638E+00,
	-8.9773540E-01,
	-3.7342377E-01,
	-8.6632643E-02,
	-1.0450598E-02,
	-5.1920577E-04
};

const double TypeKTable1[] =
{
	 0.000000E+00,
	 2.508355E+01,
	 7.860106E-02,
	-2.503131E-01,
	 8.315270E-02,
	-1.228034E-02,
	 9.804036E-04,
	-4.413030E-05,
	 1.057734E-06,
	-1.052755E-08
};

const double TypeKTable2[] =
{
	-1.318058E+02,
	 4.830222E+01,
	-1.646031E+00,
	 5.464731E-02,
	-9.650715E-04,
	 8.802193E-06,
	-3.110810E-08
};

const NIST_Table_type TypeKTables[] =
{
	{
		9,
		0.00,
		TypeKTable0
	},
	{
		10,
		20.644,
		TypeKTable1
	},
	{
		7,
		54.886,
		TypeKTable2
	}
};

const double TypeKReverse[] =
{
	-0.176004136860E-01,
	 0.389212049750E-01,
	 0.185587700320E-04,
	-0.994575928740E-07,
	 0.318409457190E-09,
	-0.560728448890E-12,
	 0.560750590590E-15,
	-0.320207200030E-18,
	 0.971511471520E-22,
	-0.121047212750E-25
};

const double TypeKReverseExtra[] =
{
	 0.118597600000E+00,
	-0.118343200000E-03,
	 0.126968600000E+03
};

const NIST_Reverse_type TypeKReverseTable = 
{
	10,						// nCoefficients
	TypeKReverse
};

// ****************************************************************************
// Type R data
const double TypeRTable0[] =
{
	 0.0000000E+00,
	 1.8891380E+02,
	-9.3835290E+01,
	 1.3068619E+02,
	-2.2703580E+02,
	 3.5145659E+02,
	-3.8953900E+02,
	 2.8239471E+02,
	-1.2607281E+02,
	 3.1353611E+01,
	-3.3187769E+00
};
const double TypeRTable1[] =
{
	 1.334584505E+01,
	 1.472644573E+02,
	-1.844024844E+01,
	 4.031129726E+00,
	-6.249428360E-01,
	 6.468412046E-02,
	-4.458750426E-03,
	 1.994710149E-04,
	-5.313401790E-06,
	 6.481976217E-08,
};
const double TypeRTable2[] =
{
	-8.199599416E+01,
	 1.553962042E+02,
	-8.342197663E+00,
	 4.279433549E-01,
	-1.191577910E-02,
	 1.492290091E-04
};
const double TypeRTable3[] =
{
	 3.406177836E+04,
	-7.023729171E+03,
	 5.582903813E+02,
	-1.952394635E+01,
	 2.560740231E-01
};

const NIST_Table_type TypeRTables[] =
{
	{
		11,
		1.923,
		TypeRTable0
	},
	{
		10,
		13.228,
		TypeRTable1
	},
	{
		6,
		19.739,
		TypeRTable2
	},
	{
		5,
		21.103,
		TypeRTable3
	}
};

const double TypeRReverse[] =
{
	 0.000000000000E+00,
	 0.528961729765E-02,
	 0.139166589782E-04,
	-0.238855693017E-07,
	 0.356916001063E-10,
	-0.462347666298E-13,
	 0.500777441034E-16,
	-0.373105886191E-19,
	 0.157716482367E-22,
	-0.281038625251E-26
};

const NIST_Reverse_type TypeRReverseTable = 
{
	10,						// nCoefficients
	TypeRReverse
};

// ****************************************************************************
// Type S data
const double TypeSTable0[] =
{
	 0.00000000E+00,
	 1.84949460E+02,
	-8.00504062E+01,
	 1.02237430E+02,
	-1.52248592E+02,
	 1.88821343E+02,
	-1.59085941E+02,
	 8.23027880E+01,
	-2.34181944E+01,
	 2.79786260E+00
};
const double TypeSTable1[] =
{
	 1.291507177E+01,
	 1.466298863E+02,
	-1.534713402E+01,
	 3.145945973E+00,
	-4.163257839E-01,
	 3.187963771E-02,
	-1.291637500E-03,
	 2.183475087E-05,
	-1.447379511E-07,
	 8.211272125E-09
};
const double TypeSTable2[] =
{
	-8.087801117E+01,
	 1.621573104E+02,
	-8.536869453E+00,
	 4.719686976E-01,
	-1.441693666E-02,
	 2.081618890E-04
};
const double TypeSTable3[] =
{
	 5.333875126E+04,
	-1.235892298E+04,
	 1.092657613E+03,
	-4.265693686E+01,
	 6.247205420E-01
};

const NIST_Table_type TypeSTables[4] =
{
	{
		10,
		1.874,
		TypeSTable0
	},
	{
		10,
		11.950,
		TypeSTable1
	},
	{
		6,
		17.536,
		TypeSTable2
	},
	{
		5,
		18.693,
		TypeSTable3
	}
};

const double TypeSReverse[] =
{
	 0.000000000000E+00,
	 0.540313308631E-02,
	 0.125934289740E-04,
	-0.232477968689E-07,
	 0.322028823036E-10,
	-0.331465196389E-13,
	 0.255744251786E-16,
	-0.125068871393E-19,
	 0.271443176145E-23
};

const NIST_Reverse_type TypeSReverseTable = 
{
	9,						// nCoefficients
	TypeSReverse
};

// ****************************************************************************
// Type T data
const double TypeTTable0[] =
{
	 0.0000000E+00,
	 2.5949192E+01,
	-2.1316967E-01,
	 7.9018692E-01,
	 4.2527777E-01,
	 1.3304473E-01,
	 2.0241446E-02,
	 1.2668171E-03
};
const double TypeTTable1[] =
{
	 0.000000E+00,
	 2.592800E+01,
	-7.602961E-01,
	 4.637791E-02,
	-2.165394E-03,
	 6.048144E-05,
	-7.293422E-07
};
const NIST_Table_type TypeTTables[2] =
{
	{
		8,
		0.00,
		TypeTTable0
	},
	{
		7,
		20.872,
		TypeTTable1
	}
};

const double TypeTReverse[] =
{						// 
	 0.000000000000E+00,
	 0.387481063640E-01,
	 0.332922278800E-04,
	 0.206182434040E-06,
	-0.218822568460E-08,
	 0.109968809280E-10,
	-0.308157587720E-13,
	 0.454791352900E-16,
	-0.275129016730E-19
};

const NIST_Reverse_type TypeTReverseTable = 
{
	9,						// nCoefficients
	TypeTReverse
};

// ****************************************************************************
// Type N data
const double TypeNTable0[] =
{
	 0.0000000E+00,
	 3.8436847E+01,
	 1.1010485E+00,
	 5.2229312E+00,
	 7.2060525E+00,
	 5.8488586E+00,
	 2.7754916E+00,
	 7.7075166E-01,
	 1.1582665E-01,
	 7.3138868E-03
};
const double TypeNTable1[] =
{
	 0.00000E+00,
	 3.86896E+01,
	-1.08267E+00,
	 4.70205E-02,
	-2.12169E-06,
	-1.17272E-04,
	 5.39280E-06,
	-7.98156E-08
};
const double TypeNTable2[] =
{
	 1.972485E+01,
	 3.300943E+01,
	-3.915159E-01,
	 9.855391E-03,
	-1.274371E-04,
	 7.767022E-07
};

const NIST_Table_type TypeNTables[3] =
{
	{
		10,
		0.00,
		TypeNTable0
	},
	{
		8,
		20.613,
		TypeNTable1
	},
	{
		6,
		47.513,
		TypeNTable2
	}
};

const double TypeNReverse[] =
{
	 0.000000000000E+00,
	 0.259293946010E-01,
	 0.157101418800E-04,
	 0.438256272370E-07,
	-0.252611697940E-09,
	 0.643118193390E-12,
	-0.100634715190E-14,
	 0.997453389920E-18,
	-0.608632456070E-21,
	 0.208492293390E-24,
	-0.306821961510E-28
};

const NIST_Reverse_type TypeNReverseTable = 
{
	11,						// nCoefficients
	TypeNReverse
};

// ****************************************************************************
// Type E data
const double TypeETable0[] =
{
	 0.0000000E+00,
	 1.6977288E+01,
	-4.3514970E-01,
	-1.5859697E-01,
	-9.2502871E-02,
	-2.6084314E-02,
	-4.1360199E-03,
	-3.4034030E-04,
	-1.1564890E-05
};
const double TypeETable1[] =
{
	 0.0000000E+00,
	 1.7057035E+01,
	-2.3301759E-01,
	 6.5435585E-03,
	-7.3562749E-05,
	-1.7896001E-06,
	 8.4036165E-08,
	-1.3735879E-09,
	 1.0629823E-11,
	-3.2447087E-14
};

const NIST_Table_type TypeETables[2] =
{
	{
		9,
		0.00,
		TypeETable0
	},
	{
		10,
		76.373,
		TypeETable1
	}
};

const double TypeEReverse[] =
{
	 0.000000000000E+00,
	 0.586655087100E-01,
	 0.450322755820E-04,
	 0.289084072120E-07,
	-0.330568966520E-09,
	 0.650244032700E-12,
	-0.191974955040E-15,
	-0.125366004970E-17,
	 0.214892175690E-20,
	-0.143880417820E-23,
	 0.359608994810E-27
};

const NIST_Reverse_type TypeEReverseTable = 
{
	11,						// nCoefficients
	TypeEReverse
};

// ****************************************************************************
// Type B data
const double TypeBTable0[] =
{
	 9.8423321E+01,
	 6.9971500E+02,
	-8.4765304E+02,
	 1.0052644E+03,
	-8.3345952E+02,
	 4.5508542E+02,
	-1.5523037E+02,
	 2.9886750E+01,
	-2.4742860E+00
};
const double TypeBTable1[] =
{
	 2.1315071E+02,
	 2.8510504E+02,
	-5.2742887E+01,
	 9.9160804E+00,
	-1.2965303E+00,
	 1.1195870E-01,
	-6.0625199E-03,
	 1.8661696E-04,
	-2.4878585E-06
};

const NIST_Table_type TypeBTables[2] =
{
	{
		9,
		2.431,
		TypeBTable0
	},
	{
		9,
		13.820,
		TypeBTable1
	}
};

const double TypeBReverse[] =
{
	 0.000000000000E+00,
	-0.246508183460E-03,
	 0.590404211710E-05,
	-0.132579316360E-08,
	 0.156682919010E-11,
	-0.169445292400E-14,
	 0.629903470940E-18
};

const NIST_Reverse_type TypeBReverseTable = 
{
	7,						// nCoefficients
	TypeBReverse
};


// ****************************************************************************

const Thermocouple_Data_type ThermocoupleData[8] =
{
	{
		3, 							// nTables
		&TypeJReverseTable, 		// Reverse Table
		TypeJTables					// Tables
	},
	{
		3, 							// nTables
		&TypeKReverseTable, 		// Reverse Table
		TypeKTables					// Tables
	},
	{
		2, 							// nTables
		&TypeTReverseTable, 		// Reverse Table
		TypeTTables					// Tables
	},
	{
		2, 							// nTables
		&TypeEReverseTable, 		// Reverse Table
		TypeETables					// Tables
	},
	{
		4, 							// nTables
		&TypeRReverseTable, 		// Reverse Table
		TypeRTables					// Tables
	},
	{
		4, 							// nTables
		&TypeSReverseTable, 		// Reverse Table
		TypeSTables					// Tables
	},
	{
		2, 							// nTables
		&TypeBReverseTable, 		// Reverse Table
		TypeBTables					// Tables
	},
	{
		3, 							// nTables
		&TypeNReverseTable, 		// Reverse Table
		TypeNTables					// Tables
	}
};

double NISTCalcVoltage(unsigned int type, double temp)
{
	unsigned int nCoef;
	unsigned int index;
	double fVoltage;
	double fTemp;
	double fExtra = 0.0;

	if (type > TC_TYPE_N)
    {
        type = TC_TYPE_N;
    }
	
	// select appropriate NIST table data
	nCoef = ThermocoupleData[type].ReverseTable->nCoefficients;
	
	// calc V
	if (type == TC_TYPE_K)
	{
		// extra calcs for type K
		fTemp = temp - TypeKReverseExtra[2];
		fTemp *= fTemp;
		fTemp *= TypeKReverseExtra[1];
		fExtra = exp(fTemp);
		fExtra *= TypeKReverseExtra[0];
	}

	fTemp = 1.0;
	fVoltage = ThermocoupleData[type].ReverseTable->Coefficients[0];
	for (index = 1; index < nCoef; index++)
	{
		fTemp *= temp;
		fVoltage += fTemp *
			ThermocoupleData[type].ReverseTable->Coefficients[index];
	}

	if (type == TC_TYPE_K)
	{
		fVoltage += fExtra;
	}

	return fVoltage;
}

double NISTCalcTemperature(unsigned int type, double voltage)
{
	unsigned int index;
	unsigned int num;
	unsigned int mytable;
	double fVoltage;
	double fResult;
	
	if (type > TC_TYPE_N)
	{
		type = TC_TYPE_N;
	}

	// determine which temp range table to use with the threshold V
	num = ThermocoupleData[type].nTables;
	index = 0;
	mytable = 0;
	
	while ((index < num) &&
		(voltage > ThermocoupleData[type].Tables[index].VThreshold))
	{
		index++;
	}
	
	if (index == num)
	{
		mytable = index - 1;
	}
	else
	{
		mytable = index;
	}
	
	// calculate T using NIST table
	num = ThermocoupleData[type].Tables[mytable].nCoefficients;
	fVoltage = 1.0;
	fResult = ThermocoupleData[type].Tables[mytable].Coefficients[0];
	for (index = 1; index < num; index++)
	{
		fVoltage *= voltage;
		fResult += fVoltage *
			ThermocoupleData[type].Tables[mytable].Coefficients[index];
	}
	
	return fResult;
}
