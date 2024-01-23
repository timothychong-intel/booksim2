/*
 * swm_inj.hpp
 *
 * Scalable workload model traffic generator for Booksim
 *
 *  Created on: Jun 9, 2022
 *      Author: cjbeckma and hdogan
 */

#ifndef SRC_SWM_INJ_HPP_
#define SRC_SWM_INJ_HPP_

#include <iostream>
#include <vector>
#include <map>
#include <boost/coroutine/all.hpp>
#include "injection.hpp"
#include "swm.hpp"
#include "roi.hpp"
#include "wkld_comp.hpp"


class ComponentInjectionProcess : public InjectionProcess {
    WorkloadComponent * _upstream;
  public:
    bool test(int src)               { return _upstream->test(src); }
    WorkloadMessagePtr get(int src)  { return _upstream->get(src); }
    void next(int src)               { _upstream->next(src); }
    void eject(WorkloadMessagePtr m) { _upstream->eject(m); }

	ComponentInjectionProcess(int nodes, const string& params, Configuration const * const config);

    virtual ~ComponentInjectionProcess();
  private:
    void _ParseComp(const string &, string &, vector<string> &);
};


#endif /* SRC_SWM_INJ_HPP_ */
