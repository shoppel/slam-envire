#ifndef TREEVIEWLISTENER_H
#define TREEVIEWLISTENER_H

#include <envire/Core.hpp>
#include <envire/core/EventHandler.hpp>
#include <QAbstractItemModel>
#include <QTreeWidget>
#include <QList>
#include <QObject>

namespace envire {

class TreeViewListener : public QObject, public envire::EventListener
{
    Q_OBJECT

    public:
	TreeViewListener(QTreeWidget *tw);
	virtual void childAdded(envire::FrameNode* parent, envire::FrameNode* child);
	virtual void childRemoved(envire::FrameNode* parent, envire::FrameNode* child);
	virtual void itemDetached( envire::EnvironmentItem* item );
        
	virtual void frameNodeSet(envire::CartesianMap* map, envire::FrameNode* node);
	virtual void frameNodeDetached(envire::CartesianMap* map, envire::FrameNode* node);

	virtual void setRootNode(envire::FrameNode* root);
	virtual void removeRootNode(envire::FrameNode* root);
	
	envire::EnvironmentItem* getItemForWidget(QTreeWidgetItem *widget) {
	    if(widgetToNode.count(widget))
		return widgetToNode[widget];
	    
	    return NULL;
	}
	
	QList<QTreeWidgetItem*> getSelectetWidgets();
	
	QTreeWidget *tw;

	envire::EnvironmentItem *selected;

    public slots:
	void itemClicked ( QTreeWidgetItem * item, int column );

    private:
	enum ItemTypes {
	    FrameNode,
	    CartesianMap,
	};
	QTreeWidgetItem *getWidgetForEnvItem(envire::EnvironmentItem* item, ItemTypes type, bool topLevel = false);
	
	std::map<envire::EnvironmentItem *, QTreeWidgetItem *> nodeToWidget;
	std::map<QTreeWidgetItem *, envire::EnvironmentItem *> widgetToNode;
};

}
#endif // TREEVIEWLISTENER_H
