#include <CCA/Components/Arches/Transport/TransportFactory.h>
#include <CCA/Components/Arches/Transport/KScalarRHS.h>
#include <CCA/Components/Arches/Transport/KMomentum.h>
#include <CCA/Components/Arches/Transport/ComputePsi.h>
#include <CCA/Components/Arches/Transport/KFEUpdate.h>
#include <CCA/Components/Arches/Task/TaskInterface.h>

namespace Uintah{

TransportFactory::TransportFactory()
{}

TransportFactory::~TransportFactory()
{}

void
TransportFactory::register_all_tasks( ProblemSpecP& db )
{

  if ( db->findBlock("KScalarTransport") ){

    ProblemSpecP db_st = db->findBlock("KScalarTransport");

    for (ProblemSpecP eqn_db = db_st->findBlock("eqn_group"); eqn_db != 0;
         eqn_db = eqn_db->findNextBlock("eqn_group")){

      std::string group_name = "null";
      std::string type = "null";
      eqn_db->getAttribute("label", group_name);
      eqn_db->getAttribute("type", type );

      TaskInterface::TaskBuilder* tsk;
      if ( type == "CC" ){
        tsk = scinew KScalarRHS<CCVariable<double> >::Builder(group_name, 0);
      } else if ( type == "FX" ){
        tsk = scinew KScalarRHS<SFCXVariable<double> >::Builder(group_name, 0);
      } else if ( type == "FY" ){
        tsk = scinew KScalarRHS<SFCYVariable<double> >::Builder(group_name, 0);
      } else if ( type == "FZ" ){
        tsk = scinew KScalarRHS<SFCZVariable<double> >::Builder(group_name, 0);
      } else {
        throw InvalidValue("Error: Eqn type for group not recognized named: "+group_name+" with type: "+type,__FILE__,__LINE__);
      }
      _scalar_builders.push_back(group_name);
      register_task( group_name, tsk );

      //Generate a psi function for each scalar and fe updates:
      std::string compute_psi_name = "compute_scalar_psi_"+group_name;
      std::string update_task_name = "scalar_fe_update_"+group_name;
      if ( type == "CC" ){
        TaskInterface::TaskBuilder* compute_psi_tsk =
        scinew ComputePsi<CCVariable<double> >::Builder( compute_psi_name, 0 );
        register_task( compute_psi_name, compute_psi_tsk );
        KFEUpdate<CCVariable<double> >::Builder* update_tsk =
        scinew KFEUpdate<CCVariable<double> >::Builder( update_task_name, 0 );
        register_task( update_task_name, update_tsk );
      } else if ( type == "FX" ){
        TaskInterface::TaskBuilder* compute_psi_tsk =
        scinew ComputePsi<SFCXVariable<double> >::Builder( compute_psi_name, 0 );
        register_task( compute_psi_name, compute_psi_tsk );
        KFEUpdate<SFCXVariable<double> >::Builder* update_tsk =
        scinew KFEUpdate<SFCXVariable<double> >::Builder( update_task_name, 0 );
        register_task( update_task_name, update_tsk );
      } else if ( type == "FY" ){
        TaskInterface::TaskBuilder* compute_psi_tsk =
        scinew ComputePsi<SFCYVariable<double> >::Builder( compute_psi_name, 0 );
        register_task( compute_psi_name, compute_psi_tsk );
        KFEUpdate<SFCYVariable<double> >::Builder* update_tsk =
        scinew KFEUpdate<SFCYVariable<double> >::Builder( update_task_name, 0 );
        register_task( update_task_name, update_tsk );
      } else if ( type == "FZ" ){
        TaskInterface::TaskBuilder* compute_psi_tsk =
        scinew ComputePsi<SFCZVariable<double> >::Builder( compute_psi_name, 0 );
        register_task( compute_psi_name, compute_psi_tsk );
        KFEUpdate<SFCZVariable<double> >::Builder* update_tsk =
        scinew KFEUpdate<SFCZVariable<double> >::Builder( update_task_name, 0 );
        register_task( update_task_name, update_tsk );
      }
      _scalar_compute_psi.push_back(compute_psi_name);
      _scalar_update.push_back( update_task_name );

      // std::string ssp_task_name = "scalar_ssp_update";
      // SSPInt::Builder* tsk2 = scinew SSPInt::Builder( ssp_task_name, 0, _scalar_builders );
      // register_task( ssp_task_name, tsk2 );

      // _scalar_ssp.push_back( ssp_task_name );

    }
  }

  if ( db->findBlock("KMomentum") ){

    // X-mom
    std::string compute_psi_name = "x-mom-psi";
    std::string update_task_name = "x-mom-update";
    std::string mom_task_name = "x-mom";
    TaskInterface::TaskBuilder* x_tsk = scinew KMomentum<SFCXVariable<double> >::Builder(mom_task_name, 0);
    register_task( mom_task_name, x_tsk );
    TaskInterface::TaskBuilder* x_compute_psi_tsk =
    scinew ComputePsi<SFCXVariable<double> >::Builder( compute_psi_name, 0 );
    register_task( compute_psi_name, x_compute_psi_tsk );
    KFEUpdate<SFCXVariable<double> >::Builder* x_update_tsk =
    scinew KFEUpdate<SFCXVariable<double> >::Builder( update_task_name, 0 );
    register_task( update_task_name, x_update_tsk );
    _momentum_builders.push_back(mom_task_name);
    _momentum_compute_psi.push_back(compute_psi_name);
    _momentum_update.push_back(update_task_name);

    // Y-mom
    compute_psi_name = "y-mom-psi";
    update_task_name = "y-mom-update";
    mom_task_name = "y-mom";
    TaskInterface::TaskBuilder* y_tsk = scinew KMomentum<SFCYVariable<double> >::Builder(mom_task_name, 0);
    register_task( mom_task_name, y_tsk );
    TaskInterface::TaskBuilder* y_compute_psi_tsk =
    scinew ComputePsi<SFCYVariable<double> >::Builder( compute_psi_name, 0 );
    register_task( compute_psi_name, y_compute_psi_tsk );
    TaskInterface::TaskBuilder* y_update_tsk =
    scinew KFEUpdate<SFCYVariable<double> >::Builder( update_task_name, 0 );
    register_task( update_task_name, y_update_tsk );
    _momentum_builders.push_back(mom_task_name);
    _momentum_compute_psi.push_back(compute_psi_name);
    _momentum_update.push_back(update_task_name);

    // Z-mom
    compute_psi_name = "z-mom-psi";
    update_task_name = "z-mom-update";
    mom_task_name = "z-mom";
    TaskInterface::TaskBuilder* z_tsk = scinew KMomentum<SFCZVariable<double> >::Builder(mom_task_name, 0);
    register_task( mom_task_name, z_tsk );
    TaskInterface::TaskBuilder* z_compute_psi_tsk =
    scinew ComputePsi<SFCZVariable<double> >::Builder( compute_psi_name, 0 );
    register_task( compute_psi_name, z_compute_psi_tsk );
    TaskInterface::TaskBuilder* z_update_tsk =
    scinew KFEUpdate<SFCZVariable<double> >::Builder( update_task_name, 0 );
    register_task( update_task_name, z_update_tsk );
    _momentum_builders.push_back(mom_task_name);
    _momentum_compute_psi.push_back(compute_psi_name);
    _momentum_update.push_back(update_task_name);

  }
}

void
TransportFactory::build_all_tasks( ProblemSpecP& db )
{

  if ( db->findBlock("KScalarTransport") ){

    ProblemSpecP db_st = db->findBlock("KScalarTransport");

    for (ProblemSpecP group_db = db_st->findBlock("eqn_group"); group_db != 0;
         group_db = group_db->findNextBlock("eqn_group")){

      std::string group_name = "null";
      std::string type = "null";
      group_db->getAttribute("label", group_name);
      group_db->getAttribute("type", type );

      //RHS builders
      TaskInterface* tsk = retrieve_task(group_name);
      proc0cout << "       Task: " << group_name << "  Type: " << "compute_RHS" << std::endl;
      tsk->problemSetup(group_db);
      tsk->create_local_labels();

      TaskInterface* psi_tsk = retrieve_task("compute_scalar_psi_"+group_name);
      proc0cout << "       Task: " << group_name << "  Type: " << "compute_psi" << std::endl;
      psi_tsk->problemSetup( group_db );
      psi_tsk->create_local_labels();

      TaskInterface* fe_tsk = retrieve_task("scalar_fe_update_"+group_name);
      proc0cout << "       Task: " << group_name << "  Type: " << "scalar_fe_update" << std::endl;
      fe_tsk->problemSetup( group_db );
      fe_tsk->create_local_labels();

      // tsk = retrieve_task("scalar_ssp_update_"+group_name);
      // tsk->problemSetup( group_db );
      //
      // tsk->create_local_labels();

    }
  }

  ProblemSpecP db_mom = db->findBlock("KMomentum");

  if ( db_mom != 0 ){

    TaskInterface* tsk = retrieve_task( "x-mom" );
    print_task_setup_info( "x-mom-compute-rhs", "compute rhs");
    tsk->problemSetup( db_mom );
    tsk->create_local_labels();

    TaskInterface* psi_tsk = retrieve_task("x-mom-psi");
    print_task_setup_info( "x-mom-compute-psi", "compute psi");
    psi_tsk->problemSetup( db_mom );
    psi_tsk->create_local_labels();

    TaskInterface* fe_tsk = retrieve_task("x-mom-update");
    print_task_setup_info( "x-mom-update", "fe update");
    fe_tsk->problemSetup( db_mom );
    fe_tsk->create_local_labels();

    tsk = retrieve_task( "y-mom" );
    print_task_setup_info( "y-mom-compute-rhs", "compute rhs");
    tsk->problemSetup( db_mom );
    tsk->create_local_labels();

    psi_tsk = retrieve_task("y-mom-psi");
    print_task_setup_info( "y-mom-compute-psi", "compute psi");
    psi_tsk->problemSetup( db_mom );
    psi_tsk->create_local_labels();

    fe_tsk = retrieve_task("y-mom-update");
    print_task_setup_info( "y-mom-update", "fe update");
    fe_tsk->problemSetup( db_mom );
    fe_tsk->create_local_labels();

    tsk = retrieve_task( "z-mom" );
    print_task_setup_info( "z-mom-compute-rhs", "compute rhs");
    tsk->problemSetup( db_mom );
    tsk->create_local_labels();

    psi_tsk = retrieve_task("z-mom-psi");
    print_task_setup_info( "z-mom-compute-psi", "compute psi");
    psi_tsk->problemSetup( db_mom );
    psi_tsk->create_local_labels();

    fe_tsk = retrieve_task("z-mom-update");
    print_task_setup_info( "z-mom-update", "fe update");
    fe_tsk->problemSetup( db_mom );
    fe_tsk->create_local_labels();

  }
}

//--------------------------------------------------------------------------------------------------
void TransportFactory::schedule_initialization( const LevelP& level,
                                                SchedulerP& sched,
                                                const MaterialSet* matls,
                                                bool doing_restart ){

  for ( auto i = _tasks.begin(); i != _tasks.end(); i++ ){

    TaskInterface* tsk = retrieve_task( i->first );
    tsk->schedule_init( level, sched, matls, doing_restart );

  }
}

} //namespace Uintah
