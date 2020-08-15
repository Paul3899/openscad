#include "evalcontext.h"
#include "UserModule.h"
#include "ModuleInstantiation.h"
#include "expression.h"
#include "function.h"
#include "printutils.h"
#include "builtin.h"
#include "localscope.h"
#include "exceptions.h"
#include "boost-utils.h"

EvalContext::EvalContext(const std::shared_ptr<Context> parent, const AssignmentList &args, const Location &loc, const class LocalScope *const scope)
	: Context(parent), loc(loc), eval_arguments(args), scope(scope)
{
}

const std::string &EvalContext::getArgName(size_t i) const
{
	assert(i < this->eval_arguments.size());
	return this->eval_arguments[i]->getName();
}

ValuePtr EvalContext::getArgValue(size_t i, const std::shared_ptr<Context> ctx) const
{
	assert(i < this->eval_arguments.size());
	const auto &arg = this->eval_arguments[i];
	ValuePtr v;
	if (arg->getExpr()) {
		v = arg->getExpr()->evaluate(ctx ? ctx : (const_cast<EvalContext *>(this))->get_shared_ptr());
	}
	return v;
}

/*!
  Resolves arguments specified by evalctx, using args to lookup positional arguments.
  optargs is for optional arguments that are not positional arguments.
  Returns an AssignmentMap (string -> Expression*)
*/
AssignmentMap EvalContext::resolveArguments(const AssignmentList &args, const AssignmentList &optargs, bool silent) const
{
  AssignmentMap resolvedArgs;
  size_t posarg = 0;
  bool tooManyWarned=false;
  // Iterate over positional args
  for (size_t i=0; i<this->numArgs(); ++i) {
    const auto &name = this->getArgName(i); // name is optional
    const auto expr = this->getArgs()[i]->getExpr().get();
    if (!name.empty()) {
      if (name.at(0)!='$' && !silent) {
        bool found=false;
        for (auto const& arg: args) {
          if (arg->getName() == name) found = true;
        }
        for (auto const& arg: optargs) {
          if (arg->getName() == name) found = true;
        }
        if (!found) {
          //"WARNING: variable %s not specified as parameter, %s", name % this->loc.toRelativeString(this->documentPath()));
        }
      }
      if (resolvedArgs.find(name) != resolvedArgs.end()) {
          LOG(boostfs_uncomplete(this->loc.filePath(),this->documentPath()).generic_string(),this->loc.firstLine(),getFormatted("argument %1$s supplied more than once",name),message_group::Warning);
      }
      resolvedArgs[name] = expr;
    }
    // If positional, find name of arg with this position
    else if (posarg < args.size()) resolvedArgs[args[posarg++]->getName()] = expr;
    else if (!silent && !tooManyWarned){
      LOG(boostfs_uncomplete(this->loc.filePath(),this->documentPath()).generic_string(),this->loc.firstLine(),getFormatted("Too many unnamed arguments supplied"),message_group::Warning);
      tooManyWarned=true;
    }
  }
  return resolvedArgs;
}

size_t EvalContext::numChildren() const
{
	return this->scope ? this->scope->children_inst.size() : 0;
}

shared_ptr<ModuleInstantiation> EvalContext::getChild(size_t i) const
{
	return this->scope ? this->scope->children_inst[i] : nullptr;
}

void EvalContext::assignTo(std::shared_ptr<Context> target) const
{
	for (const auto &assignment : this->eval_arguments) {
		ValuePtr v;
		if (assignment->getExpr()) v = assignment->getExpr()->evaluate(target);
		
		if (assignment->getName().empty()){
			LOG(boostfs_uncomplete(this->loc.filePath(),target->documentPath()).generic_string(),this->loc.firstLine(),getFormatted("Assignment without variable name %1$s",v->toEchoString()),message_group::Warning);
		} else if (target->has_local_variable(assignment->getName())) {
			LOG(boostfs_uncomplete(this->loc.filePath(),target->documentPath()).generic_string(),this->loc.firstLine(),getFormatted("Ignoring duplicate variable assignment %1$s = %2$s",assignment->getName(),v->toEchoString()),message_group::Warning);
		} else {
			target->set_variable(assignment->getName(), v);
		}
	}
}

std::ostream &operator<<(std::ostream &stream, const EvalContext &ec)
{
	for (size_t i = 0; i < ec.numArgs(); ++i) {
		if (i > 0) stream << ", ";
		if (!ec.getArgName(i).empty()) stream << ec.getArgName(i) << " = ";
		auto val = ec.getArgValue(i);
		stream << val->toEchoString();
	}
	return stream;
}

#ifdef DEBUG
std::string EvalContext::dump(const AbstractModule *mod, const ModuleInstantiation *inst)
{
	std::ostringstream s;
	if (inst)
		s << boost::format("EvalContext %p (%p) for %s inst (%p)") % this % this->parent % inst->name() % inst;
	else
		s << boost::format("Context: %p (%p)") % this % this->parent;
	s << boost::format("  document path: %s") % *this->document_path;

	s << boost::format("  eval args:");
	for (size_t i=0; i<this->eval_arguments.size(); ++i) {
		s << boost::format("    %s = %s") % this->eval_arguments[i]->getName() % this->eval_arguments[i]->getExpr();
	}
	if (this->scope && this->scope->children_inst.size() > 0) {
		s << boost::format("    children:");
		for(const auto &ch : this->scope->children_inst) {
			s << boost::format("      %s") % ch->name();
		}
	}
	if (mod) {
		const UserModule *m = dynamic_cast<const UserModule*>(mod);
		if (m) {
			s << boost::format("  module args:");
			for(const auto &arg : m->definition_arguments) {
				s << boost::format("    %s = %s") % arg->getName() % *(variables[arg->getName()]);
			}
		}
	}
	return s.str();
}
#endif

