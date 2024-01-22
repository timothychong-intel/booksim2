/*
 * swm_inj.cpp
 *
 * Booksim scalable workload model implementation
 *
 *  Created on: Jun 9, 2022
 *      Author: cjbeckma and hdogan
 */
#include "comp_inj.hpp"


// split a string by commas "," but not within parentheses.
// copied from tokenize_str in config_utils.cpp, and modified
vector<string> str_comma_split(string const & data)
{
    vector<string> values;
    if(data.empty())
        return values;

    size_t start = 0;
    int nested = 0;

    size_t curr = start;
    while(string::npos != (curr = data.find_first_of("(,)", curr))) {
        if(data[curr] == '(') {
            ++nested;
        } else if((data[curr] == ')') && nested) {
            --nested;
        } else if(!nested) {
            if(curr > start)
                values.push_back(data.substr(start, curr - start));
            start = curr + 1;
        }
        ++curr;
    }
    assert(!nested);

    if (start < data.length()) // last item
        values.push_back(data.substr(start, string::npos));

    return values;
}

// construct component injection object
ComponentInjectionProcess::ComponentInjectionProcess(
      int nodes, const string& params,
      Configuration const * const config) : InjectionProcess(nodes, vector<double>(nodes, 1.0))
{

   // Run Mode :  true  -> run to completion
   //             false -> run for specified sample period
   gSwmAppRunMode = config->GetInt("swm_app_run_mode");

   _upstream = 0;
   auto paramv = str_comma_split(params);

   // each item in the vector of strings is a component specifier, e.g. "SWM(randperm)", or "Mppn", etc.
   // Build a chain of these components after parsing into names ("SWM") and options ("randperm" or "")
   for(auto &comp : paramv)
   {
      string kind;
      vector<string> options;
      _ParseComp(comp, kind, options);
      try {
         _upstream = WorkloadComponent::New(kind, nodes, options, config, _upstream);
      }
      catch (std::out_of_range &e) {
         std::cerr << "Unknown component type: " << kind << std::endl;
         ::exit(-1);
      }
   }
   _upstream->Init(nodes, config);
}


// parse a component specifier, e.g. "SWM(randperm)" into name ("SWM") and options ("randperm")
void ComponentInjectionProcess::_ParseComp(
      const string &   spec,     // input: specifier string
      string &         name,     // output: component kind string
      vector<string> & options)  // output: options
{
   options.resize(0);
   auto lp = spec.find_first_of("(");
   if (string::npos == lp) {
      name = spec;
      return;
   }
   name = spec.substr(0, lp);
   auto rp = spec.find_last_of(")");
   options = str_comma_split(spec.substr(lp+1, rp-lp-1));
}


ComponentInjectionProcess::~ComponentInjectionProcess(){
   if (_upstream != NULL) delete _upstream;
}
