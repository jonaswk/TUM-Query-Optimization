#include "CanonicalTranslator.hpp"
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
CanonicalTranslator::CanonicalTranslator(SQLParser::Result res, Database* db) : parserResult(res), db(db)
    // Constructor
{

}
//---------------------------------------------------------------------------
CanonicalTranslator::~CanonicalTranslator()
   // Destructor
{
}

void CanonicalTranslator::addOpToQueue(CanonicalTranslator::Type type, std::unique_ptr<Operator> op) {
    operatorVector.push_back(make_pair(type,move(op)));
}

unique_ptr<Operator> CanonicalTranslator::filterWhere(unique_ptr<Operator> startOp, std::pair<SQLParser::RelationAttribute,SQLParser::RelationAttribute> joinCond,std::vector<SQLParser::Relation> relations) {
	//Resolve LHS of the predicate
	cout << "LHS: " << joinCond.first.relation << "." << joinCond.first.name << "\n";
	auto matchIt = std::find_if(relations.begin(), relations.end(), [joinCond](const SQLParser::Relation& element) {
		return element.binding == joinCond.first.relation;
	});
	cout << joinCond.first.relation << " resolved to: " << matchIt->name << "\n";

	//------------------------------------------------------------------

	//Resolve RHS of the predicate
	cout << "RHS: " << joinCond.second.relation << "." << joinCond.second.name << "\n";
	matchIt = std::find_if(relations.begin(), relations.end(), [joinCond](const SQLParser::Relation& element) {
		return element.binding == joinCond.second.relation;
	});
	cout << joinCond.second.relation << " resolved to: " << matchIt->name << "\n";

	//------------------------------------------------------------------

	//Selection
	
	//unique_ptr<Operator> theOpPointer = move(operatorVector.back().second);
	//operatorVector.pop_back();

	cout << "Loading: " << joinCond.first.relation + "." + joinCond.first.name << "\n";
	const Register* attr=registerMap[joinCond.first.relation + "." + joinCond.first.name];
	const Register* attr2=registerMap[joinCond.second.relation + "." + joinCond.second.name];


	unique_ptr<Selection> start(new Selection(move(startOp),attr,attr2));
	return start;
	
}

unique_ptr<Operator> CanonicalTranslator::translate() {

    auto relations = parserResult.relations;
	auto joinConditions = parserResult.joinConditions;
	auto selections = parserResult.selections;
	auto projections = parserResult.projections;

	for(auto it : relations) {
		Table& rel = db->getTable(it.name);
		//The scan
		unique_ptr<Tablescan> scan(new Tablescan(rel));

		auto hit = std::find_if(selections.begin(), selections.end(), [it](const std::pair<SQLParser::RelationAttribute, SQLParser::Constant>& element) {
			return element.first.relation == it.binding;
		});

		if(hit != selections.end()) {
			cout << "Saving: " << it.binding + "." + hit->first.name << "\n";
			const Register* attr=scan->getOutput(hit->first.name);
			registerMap[(it.binding + "." + hit->first.name)] = attr;
		}

		auto jit = std::find_if(joinConditions.begin(), joinConditions.end(), [it](const std::pair<SQLParser::RelationAttribute, SQLParser::RelationAttribute>& element) {
			return element.first.relation == it.binding || element.second.relation == it.binding;
		});

		if(jit != joinConditions.end()) {
			cout << "Saving: " << it.binding + "." + jit->first.name << "\n";
			const Register* attr=scan->getOutput(jit->first.name);
			registerMap[(it.binding + "." + jit->first.name)] = attr;
		}
	}

	for(auto pit : projections){

		auto rit = std::find_if(relations.begin(), relations.end(), [pit](const SQLParser::Relation& element) {
			return pit.relation == element.binding;
		});

		if(rit != relations.end()) {
			Table& rel = db->getTable(rit->name);
			unique_ptr<Tablescan> scan(new Tablescan(rel));
			cout << "Saving: " << rit->binding + "." + pit.name << "\n";
			const Register* attr=scan->getOutput(pit.name);
			registerMap[(rit->binding + "." + pit.name)] = attr;
		}
	}
    for(auto it : relations) {
		//Loop over all relations of the query and look for attr=const selection
		//We care for them first, to achieve "pushed-down predicates"
		//Get the table
		Table& rel = db->getTable(it.name);
		//The scan
		unique_ptr<Tablescan> scan(new Tablescan(rel));

		auto hit = std::find_if(selections.begin(), selections.end(), [it](const std::pair<SQLParser::RelationAttribute, SQLParser::Constant>& element) {
			return element.first.relation == it.binding;
		});

		if(hit != selections.end()) {
			// Do selection now
			cout << "Match: " << hit->first.relation << "." << hit->first.name << "=" << hit->second.value << "\n";
			
			//The constant
			Register c;
			//Set depending on type:
			switch(hit->second.type) {
				case Attribute::Type::String:
					c.setString(hit->second.value);
					break;
				case Attribute::Type::Int:
					c.setInt(stoi(hit->second.value));
					break;
				case Attribute::Type::Double:
					c.setDouble(stod(hit->second.value));
					break;
				case Attribute::Type::Bool:
					std::transform(hit->second.value.begin(), hit->second.value.end(), hit->second.value.begin(), ::tolower);
    				std::istringstream is(hit->second.value);
    				bool b;
    				is >> std::boolalpha >> b;
					c.setBool(b);
					break;
			}
			//Now select
			cout << "Selecting for: " << it.binding << "." << hit->first.name << " " << c.getString() << "\n";
			
			
			/*const Register* attr=scan->getOutput(hit->first.name);
			//Save all used registers
			cout << "Saving: " << it.binding + "." + hit->first.name << "\n";
			registerMap[(it.binding + "." + hit->first.name)] = attr;
			*/
			
			const Register* attr=registerMap[(it.binding + "." + hit->first.name)];


			unique_ptr<Selection> select( new Selection(move(scan),attr,&c));

			//Put relation + selection in our "plan" (queue)
			addOpToQueue(Type::Selection, move(select));

		} else {
			// There is no predicate to be pushed down in our current translation scheme (canonical plus constant selections)
			// Just get what we need for the cross products and add to plan
			// Get the table/scan
			addOpToQueue(Type::Scan, move(scan));
		}
	}

	//Now for canonical just reduce our queue via crossproduct
	//Take first relation out and then do one cross product on top of the other
	auto firstOp = move(operatorVector.back());
	operatorVector.pop_back();

	unique_ptr<CrossProduct> cp(new CrossProduct(move(firstOp.second),move(operatorVector.back().second)));
	operatorVector.pop_back();

	cout << "Constructing Cross Products \n";
	while(!operatorVector.empty()) {
		unique_ptr<Operator> secondRel = move(operatorVector.back().second);
		operatorVector.pop_back();

		unique_ptr<CrossProduct> res(new CrossProduct(move(cp),move(secondRel)));
		cp = move(res);
	}

	operatorVector.push_back(make_pair(Type::CrossProduct,move(cp)));
	cout << "Length of vector current: " << operatorVector.size() << "\n";

	//-----------------------------------------------------------------------------------------

	//Other predicates, here: join conditions
	cout << "Next: Other predicates from the WHERE clause \n";
	unique_ptr<Operator> begin = move(operatorVector.back().second);
	for (auto joinCond : joinConditions) {
		// Move this to another method
		begin = filterWhere(move(begin),joinCond,relations);
	}

	cout << "Length of vector current: " << operatorVector.size() << "\n";

	//Finally: Projection ----------------------------------------------------

	cout << "Now: Projection \n";
	vector<const Register*> projectionVector;
	for (auto proj : projections) {
		cout << "Projecting: " << proj.relation << "." << proj.name << "\n";
		projectionVector.push_back(registerMap[proj.relation + "." + proj.name]);
	}

	unique_ptr<Projection> project(new Projection(move(begin),{registerMap["s.name"],registerMap["s.semester"]}));

	return move(project);
}