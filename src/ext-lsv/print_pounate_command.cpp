#include "base/main/main.h"
#include "base/main/mainInt.h"

#include <iostream>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include "sat/cnf/cnf.h"
#include "proof/pdr/pdrInt.h"
using namespace std;

extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
extern "C" void * Cnf_DataWriteIntoSolver( Cnf_Dat_t * p, int nFrames, int fInit );

#define POS 0
#define NEG 1
#define SAT 1
#define UNSAT -1

namespace
{

void printAigObjInfo(Aig_Man_t* aigMan){	
    //print aigObj info
    Aig_Obj_t* aigObj;
    int i;
	cout << "**********************************************" << endl;
    Aig_ManForEachObj(aigMan, aigObj, i){
        cout << "ID: " << Aig_ObjId(aigObj) << endl;
        cout << "Type: " << Aig_ObjType(aigObj) << endl;
        cout << "FaininId0: " << Aig_ObjFaninId0(aigObj) << endl;
        cout << "FaininId1: " << Aig_ObjFaninId1(aigObj) << endl;
        cout << "FaininC0: " << Aig_ObjFaninC0(aigObj) << endl;
        cout << "FaininC1: " << Aig_ObjFaninC1(aigObj) << endl;
        cout << "Lit: " << Aig_ObjToLit(aigObj) << endl;
        cout << endl;
    }
	
	cout << "print Ci" << endl;
	Aig_ManForEachCi(aigMan, aigObj, i ){
		cout << "ID: " << Aig_ObjId(aigObj) << endl;
		cout << "Lit: " << Aig_ObjToLit(aigObj) << endl;
	}

	cout << "print Co" << endl;
    Aig_ManForEachCo(aigMan, aigObj, i ){
        cout << "ID: " << Aig_ObjId(aigObj) << endl;
        cout << "Lit: " << Aig_ObjToLit(aigObj) << endl;
    }

	cout << "**********************************************" << endl;

}

void printAigStatus(Aig_Man_t* aigMan){
    // print aig status
    cout << "aigMan: " << endl;
    Aig_ManPrintStats(aigMan);
    cout << endl;

    cout << "aigObj" << endl;
    printAigObjInfo(aigMan);
    cout << endl;
}

void printCnf(Cnf_Dat_t* ntkCnf){
    //print cnf
	cout << "**********************************************" << endl;
	cout << ntkCnf->pMan->pName << endl;
	cout << "number of variables = " << ntkCnf->nVars << endl;
    cout << "number of literals = " << ntkCnf->nLiterals << endl;
    cout << "number of clauses = " << ntkCnf->nClauses << endl;
    int *pBeg, *pEnd;
	int i;
    Cnf_CnfForClause(ntkCnf, pBeg, pEnd, i ){
        for(int* pLit = pBeg; pLit != pEnd; ++pLit){
            cout << (Abc_LitIsCompl(*pLit)? "-": "") << Abc_Lit2Var(*pLit) << " ";
        }
        cout << endl;
    }
}

void printCiInfo(Abc_Obj_t* nodeCi){
	cout << "ID: " << Abc_ObjId(nodeCi) << endl;
	cout << "name: " << Abc_ObjName(nodeCi) << endl;
}
/*
Abc_Ntk_t* get1OutputNtk(Abc_Ntk_t* abcNtk, Abc_Obj_t* nodeCo){


}
*/

//main function
int Lsv_CommandPrintPOUnate( Abc_Frame_t * pAbc, int argc, char ** argv )
{
	Abc_Ntk_t* abcNtk = Abc_FrameReadNtk(pAbc);
	Abc_Ntk_t* abcNtk_1Po;
	Abc_Obj_t* nodeCo;

	int i;
	int fUseAllCis = 1;
	Abc_NtkForEachPo( abcNtk, nodeCo, i ) {
		//get single output abc_ntk
		abcNtk_1Po = Abc_NtkCreateCone( abcNtk, Abc_ObjFanin0(nodeCo), Abc_ObjName(nodeCo), fUseAllCis );
		
		//check output negative
		if (Abc_ObjFaninC0(nodeCo) ){
			Abc_NtkPo(abcNtk_1Po, 0)->fCompl0  ^= 1;
		}

		/*
		//for debug
		Abc_Obj_t* nodeCi;
		int j;
		Abc_NtkForEachPi(abcNtk, nodeCi, j){
			printCiInfo(nodeCi);
		}
		//
		*/
		
		//get aig
		Aig_Man_t* aigMan   = Abc_NtkToDar(abcNtk_1Po, 0, 0);
		//aigMan->pData = NULL;

/*		
		//for debug
		cout << endl << "print aig: " << endl;
        printAigObjInfo(aigMan);
        cout << endl;
*/		
	
		//get F cnf
		Cnf_Dat_t* ntkCnf = Cnf_Derive( aigMan, Aig_ManCoNum(aigMan) );
		Vec_Int_t * vCiIds = Cnf_DataCollectPiSatNums( ntkCnf, aigMan );
		
		cout << "CNF of F" << endl;
		printCnf(ntkCnf);
		cout << endl;

		//get G cnf
		Cnf_Dat_t* cnfG = Cnf_DataDup(ntkCnf);
		Cnf_DataLift(cnfG, ntkCnf->nVars - 1);

		cout << "CNF of G" << endl;
        printCnf(cnfG);
        cout << endl;


		/*
		//for debug
		cout << "pVarNums:" << endl;
		for(int i = 0; i < aigMan->vObjs->nSize; ++i){
			cout << "abcObjID = " << i << endl;
			cout << "cnfVar = " << ntkCnf->pVarNums[i] << endl;
		}
		*/

		//init sat solver by adding F CNF 
		sat_solver* satSol = (sat_solver*)Cnf_DataWriteIntoSolver(ntkCnf, 1, 0);
		if ( satSol == NULL ){
			cout << "Error! SAT solver is null" << endl;
            continue;
        }

		//add cnf G into sat solver
		int *pBeg, *pEnd;
        int i;
		Cnf_CnfForClause(cnfG, pBeg, pEnd, i ){
			if ( !sat_solver_addclause( satSol, pBeg, pEnd ) ){
                cout << "error in adding output constrain to sat solver" << endl;
                continue;
            }
        }


		//create my constrain
/*		
		// create G = F and add it to sat solver
		int G_po_var = ntkCnf->nVars - 1;
		//cout << "G_po_var = " << G_po_var << endl;
		int *pBeg, *pEnd;
	    int i;

		
		cout << "CNF of F:" << endl;
		Cnf_CnfForClause(ntkCnf, pBeg, pEnd, i ){
			for(int* pLit = pBeg; pLit != pEnd; ++pLit){
				cout << (Abc_LitIsCompl(*pLit)? "-": "") << Abc_Lit2Var(*pLit) << " ";
			}
			cout << endl;
		}
		*/
/*
		//cout << "CNF of my G:" << endl;
		Cnf_CnfForClause(ntkCnf, pBeg, pEnd, i ){
			int nLits = pEnd - pBeg; 
			int *one_clause = ABC_ALLOC(int, nLits);
            for(int* pLit = pBeg; pLit != pEnd; ++pLit){
				one_clause[pLit - pBeg] = toLitCond(Abc_Lit2Var(*pLit) + G_po_var, Abc_LitIsCompl(*pLit));
			}
			if ( !sat_solver_addclause( satSol, one_clause, one_clause + nLits ) ){
			    cout << "error in adding output constrain to sat solver" << endl;
		        continue;
	        }
			ABC_FREE(one_clause);
			
			//print for debug
			for(int idx = 0; idx < nLits; ++idx){
                cout << (Abc_LitIsCompl(one_clause[idx])? "-": "") << Abc_Lit2Var(one_clause[idx]) << " ";
            }
            cout << endl;
			
        }
*/
		//////////////////////////////////////////
		//prepare for adding input constrain    //
		//////////////////////////////////////////
		int first_constrain_var = (ntkCnf->nVars - 1) * 2 + 1;//G_po_var * 2 + 1;
		int newest_var = first_constrain_var;
		int *pLits;
		//cout << "first_constrain_var = " << first_constrain_var << endl;

		//////////////////////////////////////////////////////////////////////////////
		// create an array to store the constrain control var, where                //
		// index means the var of idx-th pi, value means it's constrain control var //
		//////////////////////////////////////////////////////////////////////////////
		int *constrainSet = ABC_ALLOC(int, vCiIds->nSize);

		///////////////////////////////////////////////////////////////////////////
		//adding constrain for equality of each pi                               //
		//say C = A XNOR B                                                       //
		//if we add C in sat solver, which means we turn on the constrain A=B    //
		//if we add ~C in sat solver, we turn off the constrain                  //
		///////////////////////////////////////////////////////////////////////////
		pLits = ABC_ALLOC(int, 3);
		for(int i = 0; i < vCiIds->nSize; ++i){
			int F_pi = vCiIds->pArray[i];
			int G_pi = F_pi + (ntkCnf->nVars - 1);//F_pi + G_po_var;
			//cout << "F_pi = " << F_pi << endl;
			//cout << "G_pi = " << G_pi << endl;

			///////////////////////////////////////////
			//create C = A XNOR B and add to satSol  //
			//(~A + B + ~C)(A + ~B + ~C)             //
			///////////////////////////////////////////
			//(~A + B + ~C)
			pLits[0] = toLitCond(F_pi, NEG);
            pLits[1] = toLitCond(G_pi, POS);
            pLits[2] = toLitCond(newest_var, NEG);
			//cout << "add clause: " << pLits[0] << " + " << pLits[1] << " + " << pLits[2] << endl;
            if ( !sat_solver_addclause( satSol, pLits, pLits + 3 )){
                cout << "error in adding output constrain to sat solver" << endl;
                continue;
            }
			//(A + ~B + ~C)	
			pLits[0] = toLitCond(F_pi, POS);
            pLits[1] = toLitCond(G_pi, NEG);
            pLits[2] = toLitCond(newest_var, NEG);
			//cout << "add clause: " << pLits[0] << " + " << pLits[1] << " + " << pLits[2] << endl;
            if ( !sat_solver_addclause( satSol, pLits, pLits + 3 )){
                cout << "error in adding output constrain to sat solver" << endl;
                continue;
            }
			//store the control var in array
			constrainSet[i] = newest_var;
			newest_var ++;
		}
		ABC_FREE( pLits );
/*
		cout << "control array:" << endl;
		for(int i = 0; i < vCiIds->nSize; ++i){
			cout << "constrainSet[" << i << "] = " << constrainSet[i] << endl;
		}
*/
	
		////////////////////////////////////////////////////////////////////////
		//find pos unate                                                      //
		//make x in G = 0, and x in F = 1                                     //
		//turn on all the other control var to make other pi var be the same  //
		//add them into sat solver by satoko_solve_assumptions	              //
		//if SAT, which means we find a cex, then it is not untate            //
		//if UNSAT, this is unate function                                    //
		////////////////////////////////////////////////////////////////////////
		int nlits = 2 + vCiIds->nSize + 2;//2 po + n pi control + 2 pi
		//cout << "we will add " << nlits << " assumptions" << endl;
		pLits = ABC_ALLOC(int, nlits);
		int idx = 0;
		
		///////////	/////////////////////////////////////////////////////////
        //adding ~(~G + F) = (G)(~F) constrain, which means ~(G -> F)      //
        //if it is SAT, we find a conter example                           //
        //which means G does not imply F                                   //
		//and thus it is not unate function                                //
        /////////////////////////////////////////////////////////////////////
        int F_po = ntkCnf->pVarNums[Aig_ObjId(Aig_ManCo(aigMan, 0) )];
        //cout << "F_po = " << F_po << endl;
        pLits[idx++] = toLitCond(F_po, NEG);
        pLits[idx++] = toLitCond(F_po + (ntkCnf->nVars - 1), POS);//toLitCond(F_po + G_po_var, POS);
		
		//////////////////////////////
		//adding control constrain  //
		//////////////////////////////
		Abc_Obj_t* nodePi;
        int j;
        Abc_NtkForEachPi(abcNtk_1Po, nodePi, j){
            //printCiInfo(nodePi);
			int pi_F = ntkCnf->pVarNums[Abc_ObjId(nodePi)];
			int pi_G = pi_F + (ntkCnf->nVars - 1);//pi_F + G_po_var;
			//cout << "pi_F = " << pi_F << endl;
			//cout << "pi_G = " << pi_G << endl;

			idx = 2; //idx 0 and 1 if for adding (G)(~F)
			
			//////////////////////////////
			//set pi_F = 1 and pi_G = 0 //
			//////////////////////////////
			pLits[idx++] = toLitCond(pi_F, POS);
			pLits[idx++] = toLitCond(pi_G, NEG);
			
			////////////////////////////////////////////////////////
			//set all the other pi var be the same in F and F'    //
			////////////////////////////////////////////////////////
			for(int i = 0; i < vCiIds->nSize; ++i){
				if(i == j){//this is current considered pi, so do nothing
					//cout << "trun off constrain" << constrainSet[i] << endl;
					pLits[idx++] = toLitCond(constrainSet[i], NEG);
				}
				else{
					//cout << "turn on constrain " << constrainSet[i] << endl;
					pLits[idx++] = toLitCond(constrainSet[i], POS);
				}
		    }

			assert(idx == nlits);
			
			//solve this sat
			//note: can we simplify the problem by status = sat_solver_simplify(satSol); ?
			int status = sat_solver_solve( satSol, pLits, pLits + nlits, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );

			//result
			if ( status == SAT ){
				//cout << "sat" << endl;				
				cout << "This is not pos unate!" << endl;
			}
			else if (status == UNSAT){
				//cout << "unsat" << endl;
				cout << "This is pos unate!" << endl;
			}
			else{
				cout << "error in solveing SAT" << endl;
			}

			
		}
		ABC_FREE(pLits);

/*
		//

		//simplify the problem
		int status = sat_solver_simplify(satSol);
		if ( status == 0 ){
            printf( "The problem is UNSATISFIABLE after simplification.\n" );
			sat_solver_delete( satSol );
	        Vec_IntFree( vCiIds );
		    Cnf_DataFree(ntkCnf);
			Aig_ManStop(aigMan);
			Abc_NtkDelete(abcNtk_1Po);
            continue;
        }

		//solve sat 
		status = sat_solver_solve( satSol, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
		
		//result
		if ( status == SAT ){
            printf( "The problem is SATISFIABLE.\n" );

			//get cex assignment
			aigMan->pData = Sat_SolverGetModel( satSol, vCiIds->pArray, vCiIds->nSize );
			abcNtk_1Po->pModel = (int *)aigMan->pData;

			//check
			int * pSimInfo = Abc_NtkVerifySimulatePattern( abcNtk_1Po, abcNtk_1Po->pModel );
	        if ( pSimInfo[0] != 1 )
				Abc_Print( 1, "ERROR in Abc_NtkMiterSat(): Generated counter example is invalid.\n" );
			ABC_FREE( pSimInfo );
		   
			//create cex
			pAbc->pCex = Abc_CexCreate( 0, Abc_NtkPiNum(abcNtk_1Po), abcNtk_1Po->pModel, 0, 0, 0 );
			//Abc_CexPrint( pAbc->pCex );
		
		}
        else if ( status == UNSAT ){
			printf( "The problem is UNSATISFIABLE.\n" );
        }
		else{
			cout << "err in satsol!" << endl;
		}
*/

		//free memory
		ABC_FREE( constrainSet );
		sat_solver_delete( satSol ); //cout << "ok to free sat" << endl;
		Vec_IntFree( vCiIds ); //cout << "ok to free vCiIds" << endl;
		Cnf_DataFree(ntkCnf); //cout << "ok to free CNF" << endl;
		Aig_ManStop(aigMan); //cout << "ok to free AIG" << endl;
		//Abc_NtkDelete(abcNtk_1Po); cout << "ok to free ntk" << endl;
	}
    return 0;
}

// called during ABC startup
void init(Abc_Frame_t* pAbc)
{
    Cmd_CommandAdd( pAbc, "LSV", "lsv_print_pounate", Lsv_CommandPrintPOUnate, 0);
}

// called during ABC termination
void destroy(Abc_Frame_t* pAbc)
{
}

// this object should not be modified after the call to Abc_FrameAddInitializer
Abc_FrameInitializer_t frame_initializer = { init, destroy };

// register the initializer a constructor of a global object
// called before main (and ABC startup)
struct registrar
{
    registrar() 
    {
        Abc_FrameAddInitializer(&frame_initializer);
    }
} myAdd_registrar;

} // unnamed namespace

