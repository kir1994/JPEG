//#include <iostream>
#include <queue>
#include <map>
//#include <climits> // for CHAR_BIT
//#include <iterator>
//#include <algorithm>

//const int UniqueSymbols = 64;//1 << CHAR_BIT;

typedef std::vector<bool> HuffCode;
typedef std::map<char, HuffCode> HuffCodeMap;
typedef std::map<int, unsigned> HuffFreqTable;

class HNode
{
public:
	const int f;

	virtual ~HNode() {}

protected:
	HNode(int f) : f(f) {}
};

class InternalNode : public HNode
{
public:
	HNode *const left;
	HNode *const right;

	InternalNode(HNode *c0, HNode *c1) : HNode(c0->f + c1->f), left(c0), right(c1) {}
	~InternalNode()
	{
		delete left;
		delete right;
	}
};

class LeafNode : public HNode
{
public:
	const unsigned c;

	LeafNode(int f, unsigned c) : HNode(f), c(c) {}
};

struct NodeCmp
{
	bool operator()(const HNode *lhs, const HNode *rhs) const { return lhs->f > rhs->f; }
};

HNode* BuildTree(const HuffFreqTable& table)//const int(&frequencies)[UniqueSymbols])
{
	std::priority_queue<HNode*, std::vector<HNode*>, NodeCmp> trees;

	for (auto& i : table)
		trees.push(new LeafNode(i.first, i.second));

	while (trees.size() > 1)
	{
		HNode* childR = trees.top();
		trees.pop();

		HNode* childL = trees.top();
		trees.pop();

		HNode* parent = new InternalNode(childL, childR);
		trees.push(parent);
	}
	return trees.top();
}

void GenerateCodes(const HNode* node, const HuffCode& prefix, HuffCodeMap& outCodes)
{
	if (const LeafNode* lf = dynamic_cast<const LeafNode*>(node))
		outCodes[lf->c] = prefix;
	else if (const InternalNode* in = dynamic_cast<const InternalNode*>(node))
	{
		HuffCode leftPrefix = prefix;
		leftPrefix.push_back(false);
		GenerateCodes(in->left, leftPrefix, outCodes);

		HuffCode rightPrefix = prefix;
		rightPrefix.push_back(true);
		GenerateCodes(in->right, rightPrefix, outCodes);
	}
}