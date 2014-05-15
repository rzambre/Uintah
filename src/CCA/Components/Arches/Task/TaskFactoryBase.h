#ifndef UT_TaskFactoryBase_h
#define UT_TaskFactoryBase_h

#include <CCA/Components/Arches/Task/TaskInterface.h>
#include <string>

namespace Uintah{ 

  class TaskFactoryBase{ 

  public: 

    TaskFactoryBase(); 
    virtual ~TaskFactoryBase(); 

    typedef std::map< std::string, TaskInterface*>              TaskMap; 
    typedef std::map< std::string, TaskInterface::TaskBuilder*> BuildMap; 

    //May need to overload for the builders. 
    virtual void register_all_tasks( ProblemSpecP& db ) = 0; 

    virtual void build_all_tasks( ProblemSpecP& db ) = 0; 

    void register_task(std::string task_name, 
                       TaskInterface::TaskBuilder* builder );

    TaskInterface& retrieve_task( const std::string task_name );

  protected: 

    BuildMap  _builders;          ///< Builder map
    TaskMap   _tasks;             ///< Task map

    std::vector<std::string> _active_tasks;       ///< Task which are active 

  private: 
  
  
  
  };
}
#endif 