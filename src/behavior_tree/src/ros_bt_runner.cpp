#include "behaviortree_cpp/bt_factory.h"

#include <actionlib/client/simple_action_client.h>
#include <ros/ros.h>
#include <std_msgs/Bool.h>

#include "interfaces/ActionStatus.h"
#include "interfaces/Battery.h"
#include "interfaces/CallNurseAction.h"
#include "interfaces/FaceIdentify.h"
#include "interfaces/Fault.h"
#include "interfaces/LLMInteractionAction.h"
#include "interfaces/NavigateAction.h"
#include "interfaces/PatrolTrigger.h"
#include "interfaces/DetectAnomaly.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace BT;

namespace {

struct RosContext
{
    ros::NodeHandle nh;
    ros::ServiceClient patrol_client;
    ros::Subscriber battery_sub;
    ros::Subscriber fault_sub;
    ros::Subscriber call_signal_sub;
    ros::Subscriber patrol_trigger_sub;
    ros::ServiceClient anomaly_client;
    ros::ServiceClient face_identify_client;

    float battery_soc = 100.0f;
    bool battery_charging = false;
    float battery_voltage = 0.0f;
    std::string fault_type;
    int fault_severity = 0;
    bool call_signal = false;
    bool patrol_triggered = false;
    double battery_low_threshold = 20.0;
    int beds_per_patrol = 2;
};

Blackboard::Ptr g_root_bb;
std::shared_ptr<RosContext> g_ctx;

bool getBool(const Blackboard::Ptr& bb, const std::string& key, bool default_value = false)
{
    bool value = default_value;
    if (!bb->get(key, value))
    {
        return default_value;
    }
    return value;
}

bool getBool(const Blackboard* bb, const std::string& key, bool default_value = false)
{
    if (!bb)
    {
        return default_value;
    }
    bool value = default_value;
    if (!bb->get(key, value))
    {
        return default_value;
    }
    return value;
}

int getInt(const Blackboard::Ptr& bb, const std::string& key, int default_value = 0)
{
    int value = default_value;
    if (!bb->get(key, value))
    {
        return default_value;
    }
    return value;
}

std::string getString(const Blackboard::Ptr& bb, const std::string& key, const std::string& default_value = "")
{
    std::string value = default_value;
    if (!bb->get(key, value))
    {
        return default_value;
    }
    return value;
}

std::string getString(const Blackboard* bb, const std::string& key, const std::string& default_value = "")
{
    if (!bb)
    {
        return default_value;
    }
    std::string value = default_value;
    if (!bb->get(key, value))
    {
        return default_value;
    }
    return value;
}

void batteryCb(const interfaces::Battery::ConstPtr& msg)
{
    if (!g_ctx)
    {
        return;
    }
    g_ctx->battery_soc = msg->soc;
    g_ctx->battery_charging = msg->charging;
    g_ctx->battery_voltage = msg->voltage;
}

void faultCb(const interfaces::Fault::ConstPtr& msg)
{
    if (!g_ctx)
    {
        return;
    }
    g_ctx->fault_type = msg->fault_type;
    g_ctx->fault_severity = msg->severity;
}

void callSignalCb(const std_msgs::Bool::ConstPtr& msg)
{
    if (!g_ctx)
    {
        return;
    }
    g_ctx->call_signal = msg->data;
}

void patrolTriggerCb(const std_msgs::Bool::ConstPtr& msg)
{
    if (!g_ctx)
    {
        return;
    }
    g_ctx->patrol_triggered = msg->data;
}

bool actionOk(const interfaces::ActionStatus& status)
{
    return status.status == interfaces::ActionStatus::OK;
}

class IdleWait : public StatefulActionNode
{
public:
    IdleWait(const std::string& name, const NodeConfig& config) : StatefulActionNode(name, config) {}
    static PortsList providedPorts() { return {}; }
    NodeStatus onStart() override { return NodeStatus::RUNNING; }
    NodeStatus onRunning() override { return NodeStatus::RUNNING; }
    void onHalted() override {}
};

class NavgateTo : public StatefulActionNode
{
public:
    NavgateTo(const std::string& name, const NodeConfig& config)
        : StatefulActionNode(name, config), ac_("navigate", false)
    {
    }

    static PortsList providedPorts()
    {
        return { InputPort<std::string>("target"), InputPort<std::string>("nav_type") };
    }

    NodeStatus onStart() override
    {
        auto target = getInput<std::string>("target").value_or("unknown");
        auto nav_type = getInput<std::string>("nav_type").value_or("goal");
        if (!ac_.waitForServer(ros::Duration(1.0)))
        {
            std::cout << "[ERR ] NavgateTo no server\n";
            return NodeStatus::FAILURE;
        }
        interfaces::NavigateGoal goal;
        goal.target = target;
        goal.nav_type = nav_type;
        ac_.sendGoal(goal);
        std::cout << "[START] NavgateTo target=" << target << " type=" << nav_type << "\n";
        return NodeStatus::RUNNING;
    }

    NodeStatus onRunning() override
    {
        if (!ac_.getState().isDone())
        {
            return NodeStatus::RUNNING;
        }
        auto result = ac_.getResult();
        if (!result)
        {
            std::cout << "[ERR ] NavgateTo no result\n";
            return NodeStatus::FAILURE;
        }
        if (actionOk(result->status))
        {
            std::cout << "[DONE] NavgateTo\n";
            return NodeStatus::SUCCESS;
        }
        std::cout << "[FAIL] NavgateTo status=" << static_cast<int>(result->status.status) << "\n";
        return NodeStatus::FAILURE;
    }

    void onHalted() override
    {
        ac_.cancelGoal();
        std::cout << "[HALT] NavgateTo\n";
    }

private:
    actionlib::SimpleActionClient<interfaces::NavigateAction> ac_;
};

class LLMInteraction : public StatefulActionNode
{
public:
    LLMInteraction(const std::string& name, const NodeConfig& config)
        : StatefulActionNode(name, config), ac_("llm_interaction", false)
    {
    }

    static PortsList providedPorts()
    {
        return { InputPort<std::string>("mode"), InputPort<int>("person_id"), OutputPort<bool>("need_call_nurse") };
    }

    NodeStatus onStart() override
    {
        auto mode = getInput<std::string>("mode").value_or("unknown");
        int person_id = getInput<int>("person_id").value_or(-1);
        if (!ac_.waitForServer(ros::Duration(1.0)))
        {
            std::cout << "[ERR ] LLMInteraction no server\n";
            return NodeStatus::FAILURE;
        }
        interfaces::LLMInteractionGoal goal;
        goal.mode = mode;
        goal.person_id = person_id;
        goal.context = "";
        ac_.sendGoal(goal);
        std::cout << "[START] LLMInteraction mode=" << mode << " person_id=" << person_id << "\n";
        return NodeStatus::RUNNING;
    }

    NodeStatus onRunning() override
    {
        if (!ac_.getState().isDone())
        {
            return NodeStatus::RUNNING;
        }
        auto result = ac_.getResult();
        if (!result)
        {
            std::cout << "[ERR ] LLMInteraction no result\n";
            return NodeStatus::FAILURE;
        }
        bool need_call_nurse = result->need_call_nurse;
        setOutput("need_call_nurse", need_call_nurse);
        if (config().blackboard)
        {
            config().blackboard->set("call_nurse", need_call_nurse);
        }
        std::cout << "[DONE] LLMInteraction need_call_nurse=" << (need_call_nurse ? "true" : "false") << "\n";
        return actionOk(result->status) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }

    void onHalted() override
    {
        ac_.cancelGoal();
        std::cout << "[HALT] LLMInteraction\n";
    }

private:
    actionlib::SimpleActionClient<interfaces::LLMInteractionAction> ac_;
};

class CallDutyNurse : public StatefulActionNode
{
public:
    CallDutyNurse(const std::string& name, const NodeConfig& config)
        : StatefulActionNode(name, config), ac_("call_nurse", false)
    {
    }

    static PortsList providedPorts() { return { InputPort<std::string>("bed_id") }; }

    NodeStatus onStart() override
    {
        auto bed_id = getInput<std::string>("bed_id").value_or("unknown");
        if (!ac_.waitForServer(ros::Duration(1.0)))
        {
            std::cout << "[ERR ] CallDutyNurse no server\n";
            return NodeStatus::FAILURE;
        }
        interfaces::CallNurseGoal goal;
        goal.bed_id = bed_id;
        ac_.sendGoal(goal);
        std::cout << "[START] CallDutyNurse bed_id=" << bed_id << "\n";
        return NodeStatus::RUNNING;
    }

    NodeStatus onRunning() override
    {
        if (!ac_.getState().isDone())
        {
            return NodeStatus::RUNNING;
        }
        auto result = ac_.getResult();
        if (!result)
        {
            std::cout << "[ERR ] CallDutyNurse no result\n";
            return NodeStatus::FAILURE;
        }
        bool ok = actionOk(result->status);
        if (config().blackboard)
        {
            config().blackboard->set("nurse_connected", ok);
        }
        std::cout << "[DONE] CallDutyNurse ok=" << (ok ? "true" : "false") << "\n";
        return ok ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }

    void onHalted() override
    {
        ac_.cancelGoal();
        std::cout << "[HALT] CallDutyNurse\n";
    }

private:
    actionlib::SimpleActionClient<interfaces::CallNurseAction> ac_;
};

class SelectNextBed : public SyncActionNode
{
public:
    SelectNextBed(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return { OutputPort<std::string>("bed_id") }; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        std::vector<std::string> queue;
        if (bb->get("bed_queue", queue) && !queue.empty())
        {
            std::string bed_id = queue.front();
            queue.erase(queue.begin());
            bb->set("bed_queue", queue);
            bb->set("beds_remaining", static_cast<int>(queue.size()));
            bb->set("bed_id", bed_id);
            setOutput("bed_id", bed_id);
            std::cout << "[SET ] SelectNextBed -> bed_id=" << bed_id << " remaining=" << queue.size() << "\n";
            return NodeStatus::SUCCESS;
        }
        int remaining = getInt(bb, "beds_remaining", 0);
        int idx = getInt(bb, "bed_index", -1) + 1;
        if (remaining > 0)
        {
            bb->set("beds_remaining", remaining - 1);
            bb->set("bed_index", idx);
            std::string bed_id = "bed_" + std::to_string(idx);
            bb->set("bed_id", bed_id);
            setOutput("bed_id", bed_id);
            std::cout << "[SET ] SelectNextBed -> bed_id=" << bed_id << " remaining=" << (remaining - 1) << "\n";
            return NodeStatus::SUCCESS;
        }
        bb->set("bed_id", std::string("unknown"));
        setOutput("bed_id", std::string("unknown"));
        return NodeStatus::FAILURE;
    }
};

class FaceIdentify : public SyncActionNode
{
public:
    FaceIdentify(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return { OutputPort<int>("person_id"), OutputPort<float>("confidence") }; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        
        // 调用人脸识别服务
        interfaces::FaceIdentify srv;
        
        if (g_ctx->face_identify_client.call(srv))
        {
            // 获取识别结果
            int person_id = srv.response.person_id;
            float confidence = srv.response.confidence;
            
            bb->set("person_id", person_id);
            bb->set("face_confidence", confidence);
            setOutput("person_id", person_id);
            setOutput("confidence", confidence);
            
            if (person_id > 0)
            {
                std::cout << "[ACT ] FaceIdentify 识别成功: person_id=" << person_id << ", confidence=" << confidence << "\n";
            }
            else
            {
                std::cout << "[ACT ] FaceIdentify 未识别到已知人脸\n";
            }
            
            return NodeStatus::SUCCESS;  // 无论识别成功与否都返回 SUCCESS
        }
        else
        {
            std::cerr << "[ACT ] FaceIdentify 服务调用失败\n";
            bb->set("person_id", -1);
            bb->set("face_confidence", 0.0f);
            setOutput("person_id", -1);
            setOutput("confidence", 0.0f);
            return NodeStatus::SUCCESS;  // 失败也返回 SUCCESS，不中断流程
        }
    }
};

class EnqueueNurseCall : public SyncActionNode
{
public:
    EnqueueNurseCall(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return { InputPort<std::string>("bed_id") }; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        std::string bed_id = getInput<std::string>("bed_id").value_or(getString(bb, "bed_id", "unknown"));
        if (g_root_bb)
        {
            g_root_bb->set("nurse_call_pending", true);
            g_root_bb->set("nurse_call_bed_id", bed_id);
        }
        std::cout << "[ACT ] EnqueueNurseCall bed_id=" << bed_id << "\n";
        return NodeStatus::SUCCESS;
    }
};

class PopNurseCall : public SyncActionNode
{
public:
    PopNurseCall(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return { OutputPort<std::string>("bed_id") }; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        std::string bed_id = getString(g_root_bb.get(), "nurse_call_bed_id", "unknown");
        if (g_root_bb)
        {
            g_root_bb->set("nurse_call_pending", false);
        }
        bb->set("nurse_bed_id", bed_id);
        setOutput("bed_id", bed_id);
        std::cout << "[ACT ] PopNurseCall bed_id=" << bed_id << "\n";
        return NodeStatus::SUCCESS;
    }
};

class ClearInteractionMode : public SyncActionNode
{
public:
    ClearInteractionMode(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return {}; }

    NodeStatus tick() override
    {
        if (g_root_bb)
        {
            g_root_bb->set("interaction_mode", std::string("none"));
        }
        std::cout << "[ACT ] ClearInteractionMode\n";
        return NodeStatus::SUCCESS;
    }
};

class IsInteractionMode : public ConditionNode
{
public:
    IsInteractionMode(const std::string& name, const NodeConfig& config) : ConditionNode(name, config) {}

    static PortsList providedPorts() { return { InputPort<std::string>("mode") }; }

    NodeStatus tick() override
    {
        auto mode = getInput<std::string>("mode").value_or("unknown");
        std::string current = getString(g_root_bb.get(), "interaction_mode", "none");
        auto norm = [](std::string s) {
            s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
            if (!s.empty() && s.front() == '[') s.erase(s.begin());
            if (!s.empty() && s.back() == ']') s.pop_back();
            return s;
        };
        std::string list = norm(mode);
        if (list.find(',') == std::string::npos)
        {
            return (current == list) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        }
        size_t start = 0;
        while (start < list.size())
        {
            size_t end = list.find(',', start);
            if (end == std::string::npos) end = list.size();
            std::string item = list.substr(start, end - start);
            if (current == item)
            {
                return NodeStatus::SUCCESS;
            }
            start = end + 1;
        }
        return NodeStatus::FAILURE;
    }
};

class LoadPatrolPlan : public SyncActionNode
{
public:
    LoadPatrolPlan(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return { InputPort<std::string>("route_id"), InputPort<int>("cycles") }; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        auto route_id = getInput<std::string>("route_id").value_or("route_default");
        int cycles = getInput<int>("cycles").value_or(1);
        bb->set("patrol_route_id", route_id);
        bb->set("patrol_cycles", cycles);
        bb->set("patrol_remaining", cycles);
        bb->set("patrol_complete", false);
        std::cout << "[ACT ] LoadPatrolPlan route_id=" << route_id << " cycles=" << cycles << "\n";
        return NodeStatus::SUCCESS;
    }
};

class NextPatrolPoint : public SyncActionNode
{
public:
    NextPatrolPoint(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return { OutputPort<std::string>("patrol_point") }; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        int remaining = getInt(bb, "patrol_remaining", 0);
        if (remaining <= 0)
        {
            return NodeStatus::FAILURE;
        }
        remaining -= 1;
        bb->set("patrol_remaining", remaining);
        if (remaining == 0)
        {
            bb->set("patrol_complete", true);
        }
        int index = getInt(bb, "patrol_index", 0);
        std::string point = "p" + std::to_string(index % 2);
        bb->set("patrol_point", point);
        bb->set("patrol_index", index + 1);
        setOutput("patrol_point", point);
        std::cout << "[ACT ] NextPatrolPoint -> " << point << "\n";
        return NodeStatus::SUCCESS;
    }
};

class AlertUnfinishedOnce : public SyncActionNode
{
public:
    AlertUnfinishedOnce(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return {}; }

    NodeStatus tick() override
    {
        auto bb = config().blackboard;
        bool warned = getBool(bb, "unfinished_warned", false);
        if (!warned)
        {
            bb->set("unfinished_warned", true);
            std::cout << "[ACT ] AlertUnfinishedOnce\n";
        }
        return NodeStatus::SUCCESS;
    }
};

class WaitChargeUntil : public StatefulActionNode
{
public:
    WaitChargeUntil(const std::string& name, const NodeConfig& config) : StatefulActionNode(name, config) {}

    static PortsList providedPorts() { return { InputPort<int>("percent") }; }

    NodeStatus onStart() override
    {
        ticks_ = 0;
        target_percent_ = getInput<int>("percent").value_or(50);
        std::cout << "[START] WaitChargeUntil target=" << target_percent_ << "\n";
        return NodeStatus::RUNNING;
    }

    NodeStatus onRunning() override
    {
        ++ticks_;
        int current = ticks_ * 10;
        std::cout << "[RUN ] Charging " << current << "%\n";
        if (current >= target_percent_)
        {
            std::cout << "[DONE] Charge reached\n";
            return NodeStatus::SUCCESS;
        }
        return NodeStatus::RUNNING;
    }

    void onHalted() override {}

private:
    int ticks_ = 0;
    int target_percent_ = 50;
};

class ClearPatrolTrigger : public SyncActionNode
{
public:
    ClearPatrolTrigger(const std::string& name, const NodeConfig& config) : SyncActionNode(name, config) {}

    static PortsList providedPorts() { return {}; }

    NodeStatus tick() override
    {
        if (g_ctx)
        {
            interfaces::PatrolTrigger srv;
            srv.request.enable = false;
            if (g_ctx->patrol_client.exists())
            {
                g_ctx->patrol_client.call(srv);
            }
            g_ctx->patrol_triggered = false;
        }
        std::cout << "[ACT ] ClearPatrolTrigger\n";
        return NodeStatus::SUCCESS;
    }
};

class IsPatrolTriggered : public ConditionNode
{
public:
    IsPatrolTriggered(const std::string& name, const NodeConfig& config) : ConditionNode(name, config) {}

    static PortsList providedPorts() { return {}; }

    NodeStatus tick() override
    {
        return (g_ctx && g_ctx->patrol_triggered) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }
};

class WaitNurseConnected : public StatefulActionNode
{
public:
    WaitNurseConnected(const std::string& name, const NodeConfig& config) : StatefulActionNode(name, config) {}
    static PortsList providedPorts() { return {}; }

    NodeStatus onStart() override
    {
        std::cout << "[START] WaitNurseConnected\n";
        return NodeStatus::RUNNING;
    }

    NodeStatus onRunning() override
    {
        auto bb = config().blackboard;
        if (getBool(bb, "nurse_connected", false))
        {
            std::cout << "[DONE] Nurse connected\n";
            return NodeStatus::SUCCESS;
        }
        std::cout << "[RUN ] Waiting for nurse connection\n";
        return NodeStatus::RUNNING;
    }

    void onHalted() override {}
};

class LambdaAction : public SyncActionNode
{
public:
    using Func = std::function<NodeStatus(LambdaAction&)>;

    LambdaAction(const std::string& name, const NodeConfig& config, Func fn)
        : SyncActionNode(name, config), fn_(std::move(fn))
    {
    }

    static PortsList providedPorts()
    {
        return { InputPort<std::string>("fault_type") };
    }

    NodeStatus tick() override { return fn_(*this); }

    Blackboard::Ptr blackboard() { return config().blackboard; }

private:
    Func fn_;
};

class LambdaCondition : public ConditionNode
{
public:
    using Func = std::function<NodeStatus(LambdaCondition&)>;

    LambdaCondition(const std::string& name, const NodeConfig& config, Func fn)
        : ConditionNode(name, config), fn_(std::move(fn))
    {
    }

    static PortsList providedPorts() { return {}; }

    NodeStatus tick() override { return fn_(*this); }

    Blackboard::Ptr blackboard() { return config().blackboard; }

private:
    Func fn_;
};

} // namespace

int main(int argc, char** argv)
{
    ros::init(argc, argv, "medical_bt_ros_runner");
    ros::NodeHandle nh("~");

    g_ctx = std::make_shared<RosContext>();
    g_ctx->battery_low_threshold = nh.param("battery_low_threshold", 20.0);
    g_ctx->beds_per_patrol = nh.param("beds_per_patrol", 2);
    g_ctx->patrol_client = nh.serviceClient<interfaces::PatrolTrigger>("patrol_trigger");
    g_ctx->battery_sub = nh.subscribe("/battery", 1, batteryCb);
    g_ctx->fault_sub = nh.subscribe("/fault", 1, faultCb);
    g_ctx->call_signal_sub = nh.subscribe("/call_signal", 1, callSignalCb);
    g_ctx->patrol_trigger_sub = nh.subscribe("/patrol_triggered", 1, patrolTriggerCb);
    g_ctx->anomaly_client = nh.serviceClient<interfaces::DetectAnomaly>("detect_anomaly");
    g_ctx->face_identify_client = nh.serviceClient<interfaces::FaceIdentify>("face_identify");

    BehaviorTreeFactory factory;

    factory.registerBuilder<IdleWait>("IdleWait", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<IdleWait>(name, config);
    });

    factory.registerBuilder<NavgateTo>("NavgateTo", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<NavgateTo>(name, config);
    });

    factory.registerBuilder<LLMInteraction>("LLMInteraction", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LLMInteraction>(name, config);
    });

    factory.registerBuilder<CallDutyNurse>("CallDutyNurse", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<CallDutyNurse>(name, config);
    });

    factory.registerBuilder<SelectNextBed>("SelectNextBed", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<SelectNextBed>(name, config);
    });

    factory.registerBuilder<FaceIdentify>("FaceIdentify", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<FaceIdentify>(name, config);
    });

    factory.registerBuilder<EnqueueNurseCall>("EnqueueNurseCall", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<EnqueueNurseCall>(name, config);
    });

    factory.registerBuilder<PopNurseCall>("PopNurseCall", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<PopNurseCall>(name, config);
    });

    factory.registerBuilder<ClearInteractionMode>("ClearInteractionMode", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<ClearInteractionMode>(name, config);
    });

    factory.registerBuilder<IsInteractionMode>("IsInteractionMode", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<IsInteractionMode>(name, config);
    });

    factory.registerBuilder<LoadPatrolPlan>("LoadPatrolPlan", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LoadPatrolPlan>(name, config);
    });

    factory.registerBuilder<NextPatrolPoint>("NextPatrolPoint", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<NextPatrolPoint>(name, config);
    });


    factory.registerBuilder<AlertUnfinishedOnce>("AlertUnfinishedOnce", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<AlertUnfinishedOnce>(name, config);
    });

    factory.registerBuilder<WaitChargeUntil>("WaitChargeUntil", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<WaitChargeUntil>(name, config);
    });

    factory.registerBuilder<ClearPatrolTrigger>("ClearPatrolTrigger", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<ClearPatrolTrigger>(name, config);
    });

    factory.registerBuilder<IsPatrolTriggered>("IsPatrolTriggered", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<IsPatrolTriggered>(name, config);
    });

    factory.registerBuilder<WaitNurseConnected>("WaitNurseConnected", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<WaitNurseConnected>(name, config);
    });

    factory.registerBuilder<LambdaAction>("LoadConfig", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction& node) {
            auto bb = node.blackboard();
            bb->set("config_loaded", true);
            bb->set("patrol_started", false);
            bb->set("patrol_complete", false);
            bb->set("beds_remaining", 0);
            bb->set("bed_index", -1);
            bb->set("bed_id", std::string("unknown"));
            bb->set("anomaly", false);
            bb->set("call_nurse", false);
            bb->set("nurse_connected", false);
            bb->set("call_signal", false);
            bb->set("person_id", -1);
            bb->set("patrol_route_id", std::string("route_a"));
            bb->set("patrol_cycles", 2);
            bb->set("patrol_remaining", 0);
            bb->set("patrol_index", 0);
            bb->set("unfinished_warned", false);
            if (g_root_bb)
            {
                g_root_bb->set("nurse_call_pending", false);
                g_root_bb->set("nurse_call_bed_id", std::string("unknown"));
                g_root_bb->set("interaction_mode", std::string("none"));
            }
            std::cout << "[SET ] LoadConfig\n";
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("ManagePatrolStart", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction& node) {
            auto bb = node.blackboard();
            if (!getBool(bb, "patrol_started", false))
            {
                bb->set("patrol_started", true);
                bb->set("patrol_complete", false);
                bb->set("patrol_points_remaining", 1);
                bb->set("patrol_point", std::string("p1"));
                std::cout << "[SET ] ManagePatrolStart (init)\n";
            }
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("SaveContext", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction&) {
            std::cout << "[ACT ] SaveContext\n";
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("RestoreContext", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction&) {
            std::cout << "[ACT ] RestoreContext\n";
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("RequestAnomalyDetection", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction& node) {
            auto bb = node.blackboard();
            bb->set("beds_remaining", 2);
            bb->set("bed_index", -1);
            std::cout << "[SET ] RequestAnomalyDetection (beds=2)\n";
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("AnomalyDetect", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction& node) {
            auto bb = node.blackboard();
            std::string bed_id = getString(bb, "bed_id", "unknown");
            if (bed_id.empty() || bed_id == "unknown")
            {
                if (g_root_bb)
                {
                    g_root_bb->set("interaction_mode", std::string("passive_wakeup"));
                }
                if (g_ctx && g_ctx->anomaly_client.exists())
                {
                    interfaces::DetectAnomaly srv;
                    srv.request.mode = "scan";
                    srv.request.bed_id = "";
                    if (g_ctx->anomaly_client.call(srv))
                    {
                        std::vector<std::string> queue(srv.response.bed_ids.begin(), srv.response.bed_ids.end());
                        bb->set("bed_queue", queue);
                        bb->set("beds_remaining", static_cast<int>(queue.size()));
                        bb->set("bed_index", -1);
                        bb->set("bed_id", std::string("unknown"));
                        std::cout << "[SET ] AnomalyDetect scan beds=" << queue.size() << "\n";
                        return NodeStatus::SUCCESS;
                    }
                }
                int beds = g_ctx ? g_ctx->beds_per_patrol : 2;
                bb->set("bed_queue", std::vector<std::string>{});
                bb->set("beds_remaining", beds);
                bb->set("bed_index", -1);
                bb->set("bed_id", std::string("unknown"));
                std::cout << "[SET ] AnomalyDetect scan fallback beds=" << beds << "\n";
                return NodeStatus::SUCCESS;
            }
            bool anomaly = false;
            std::string details = "no service";

            if (g_ctx && g_ctx->anomaly_client.exists())
            {
                interfaces::DetectAnomaly srv;
                srv.request.mode = "bed";
                srv.request.bed_id = bed_id;
                if (g_ctx->anomaly_client.call(srv))
                {
                    anomaly = srv.response.is_anomaly;
                    details = srv.response.details;
                }
                else
                {
                    details = "service call failed";
                }
            }
            else
            {
                details = "service unavailable";
            }

            bb->set("anomaly", anomaly);
            if (g_root_bb)
            {
                g_root_bb->set("interaction_mode", anomaly ? std::string("abnormal_alert") : std::string("passive_wakeup"));
            }
            std::cout << "[SET ] AnomalyDetect bed=" << bed_id << " anomaly=" << (anomaly ? "true" : "false")
                      << " details=" << details << "\n";
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("AlertFault", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction& node) {
            auto fault_type = node.getInput<std::string>("fault_type").value_or("unknown");
            std::cout << "[ACT ] AlertFault type=" << fault_type << "\n";
            return NodeStatus::SUCCESS;
        });
    });

    factory.registerBuilder<LambdaAction>("AlertChargeFault", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaAction>(name, config, [](LambdaAction&) {
            std::cout << "[ACT ] AlertChargeFault\n";
            return NodeStatus::SUCCESS;
        });
    });


    factory.registerBuilder<LambdaCondition>("IsAnomaly", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition& node) {
            auto bb = node.blackboard();
            return getBool(bb, "anomaly", false) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("ShouldCallNurse", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition& node) {
            auto bb = node.blackboard();
            return getBool(bb, "call_nurse", false) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("HasNurseCall", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition&) {
            if (getBool(g_root_bb.get(), "nurse_call_pending", false))
            {
                std::cout << "[COND] HasNurseCall=true\n";
                return NodeStatus::SUCCESS;
            }
            return NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsPatrolComplete", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition& node) {
            auto bb = node.blackboard();
            return getBool(bb, "patrol_complete", false) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsPatrolIncomplete", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition& node) {
            auto bb = node.blackboard();
            return !getBool(bb, "patrol_complete", false) ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsCallSignal", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition&) {
            if (g_ctx && g_ctx->call_signal)
            {
                std::cout << "[COND] IsCallSignal=true\n";
                return NodeStatus::SUCCESS;
            }
            return NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsBatteryLow", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition&) {
            if (g_ctx && g_ctx->battery_soc <= g_ctx->battery_low_threshold)
            {
                std::cout << "[COND] IsBatteryLow=true\n";
                return NodeStatus::SUCCESS;
            }
            return NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsLocalizationAbnormal", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition&) {
            if (!g_ctx || g_ctx->fault_severity == 0)
            {
                return NodeStatus::FAILURE;
            }
            return (g_ctx->fault_type == "localization") ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsNavFault", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition&) {
            if (!g_ctx || g_ctx->fault_severity == 0)
            {
                return NodeStatus::FAILURE;
            }
            return (g_ctx->fault_type == "navigation") ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    factory.registerBuilder<LambdaCondition>("IsSelfFault", [](const std::string& name, const NodeConfig& config) {
        return std::make_unique<LambdaCondition>(name, config, [](LambdaCondition&) {
            if (!g_ctx || g_ctx->fault_severity == 0)
            {
                return NodeStatus::FAILURE;
            }
            return (g_ctx->fault_type == "self") ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        });
    });

    try
    {
        g_root_bb = Blackboard::create();
        auto tree = factory.createTreeFromFile("/home/val/BIH_ws/Medical_Embodied/src/behavior_tree/BH_xml/medical.xml", g_root_bb);

        int max_ticks = nh.param("max_ticks", 200);
        constexpr int kTickHz = 20;
        constexpr auto kTickPeriod = std::chrono::milliseconds(1000 / kTickHz);
        std::cout << "[INFO] tick_rate=" << kTickHz << "Hz period=" << kTickPeriod.count() << "ms\n";

        for (int tick = 0; tick < max_ticks && ros::ok(); ++tick)
        {
            ros::spinOnce();
            std::cout << "\n=== TICK " << tick << " ===\n";
            auto status = tree.tickOnce();
            std::cout << "[TREE] status=" << toStr(status) << "\n";
            std::this_thread::sleep_for(kTickPeriod);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
