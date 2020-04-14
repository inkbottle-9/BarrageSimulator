#include "universal.h"

///全局变量定义
RunTimeData data_runtime{};

void ObjectsControl::process_data()
{
    QReadLocker locker(&data_runtime.lock);//读锁定
//    QWriteLocker locker(&data_runtime.lock); //写锁定
//    QMutexLocker locker(&data_runtime.mutex);

    auto start = std::chrono::steady_clock::now();///------------------------------------------------------------------------------计时开始

//    Integer count=0;
//    Integer time_basic=0;//物理属性更新总体时间(时间消耗非常小, 忽略不计)
//    Integer time_collision=0;//处理碰撞总体时间(时间消耗非常大)
//    Integer time_equation=0;//解方程时间(时间消耗非常小, 忽略不计)
//    Integer time_get_list=0;//获取碰撞列表时间(时间消耗非常大, 占据总体时间消耗的99%)

    auto size=data_runtime.list_objects.size();

    ///物理计算
    for (int index = 0; index < size; ++index)
    {

        auto start0 = std::chrono::steady_clock::now();///------------------------------------------------------------------------------计时开始

        //        bool flag_max_velocity{false};//满速标记
        ///坐标计算
        FlyingObject *p_crt = data_runtime.list_objects[index];//当前对象
        auto &pro = p_crt->property;         //物理属性
        auto &acc_p = pro.acceleration_polar;   //加速度_极坐标
        auto &acc_a = pro.acceleration_axis;    //加速度_轴坐标
        auto &v_p = pro.speed_polar;            //速度_极坐标
        auto &v_a = pro.speed_axis;             //加速度_轴坐标
        auto &pos = pro.coordinate;             //位置
        auto &att_v = pro.attenuation_velocity; //速度衰减
        auto &pos_mouse = data_runtime.pos_mouse_scene;//鼠标位置

        //        emit push_info(QString::asprintf("<acc_p> %.0f %.0f <acc_a> %.0f %.0f <v_p> %.0f %.0f <v_a> %.0f %.0f",acc_p.first,acc_p.second,acc_a.first,acc_a.second,v_p.first,v_p.second,v_a.first,v_a.second));
        //        qDebug()<<QString::asprintf("<acc_p> %f %f <acc_a> %f %f <v_p> %f %f <v_a> %f %f",acc_p.first,acc_p.second,acc_a.first,acc_a.second,v_p.first,v_p.second,v_a.first,v_a.second);

        if (pro.mode_movement == MovementMode::Stop) //自动停止
        {
            acc_a.first = acc_a.second = acc_p.first = acc_p.second = 0; //清除加速度
            v_p.second -= att_v;                                         //获取当前速度大小减去速度衰减
            if (v_p.second < 0)                                          //如果速度大小为负数
                v_p.second = 0;                                          //设为0
            //根据极坐标计算轴坐标速度
            ToolFunctionsKit::polar_to_axis(v_p, v_a);

            //根据速度更新位置
            pos.first += v_a.first;   //水平位移
            pos.second += v_a.second; //垂直位移
        }
        else if (pro.mode_movement == MovementMode::Unlimited) //无限制
        {
            //根据极坐标计算轴坐标加速度
            ToolFunctionsKit::polar_to_axis(acc_p, acc_a);

            //根据极坐标计算轴坐标速度
            ToolFunctionsKit::polar_to_axis(v_p, v_a);

            //计算加速后的轴坐标速度
            v_a.first += acc_a.first;
            v_a.second += acc_a.second;

            //根据轴坐标更新极坐标速度
            ToolFunctionsKit::axis_to_polar(v_a, v_p);

            if (pro.velocity_max > 0 && v_p.second > pro.velocity_max) //最大速度限制
            {
                v_p.second = pro.velocity_max;
                //重新计算轴坐标
                ToolFunctionsKit::polar_to_axis(v_p, v_a);
            }

            //根据速度更新位置
            pos.first += v_a.first;   //水平位移
            pos.second += v_a.second; //垂直位移
        }

        if (pro.flag_boundary_restriction) //边界检查
        {
            if (pos.first < 0)
            {
                pos.first = 1;
                v_a.first = 0;
            }
            if (pos.first > data_runtime.scene_main->width())
            {
                pos.first = data_runtime.scene_main->width()-1;
                v_a.first = 0;
            }
            if (pos.second < 0)
            {
                pos.second = 1;
                v_a.second = 0;
            }
            if (pos.second > data_runtime.scene_main->height())
            {
                pos.second = data_runtime.scene_main->height()-1;
                v_a.second = 0;
            }
        }

        ToolFunctionsKit::axis_to_polar(v_a, v_p);

        ///角度计算
        switch (pro.mode_rotation)
        {
        case RotationMode::Fixed: //固定无法旋转
            break;
        case RotationMode::FollowSpeed: //跟随速度
        {
            if (pro.speed_polar.second > DBL_EPSILON || pro.speed_polar.second < -DBL_EPSILON)
                pro.rotation.first = (pro.speed_polar.first) * R2D + pro.offset_front;
            break;
        }
        case RotationMode::FollwoAcceleration: //跟随加速度
        {
            if (pro.acceleration_polar.second > DBL_EPSILON || pro.acceleration_polar.second < -DBL_EPSILON)
                pro.rotation.first = pro.acceleration_polar.first * R2D + pro.offset_front;
            break;
        }
        case RotationMode::TowardsMouse:
        {
            //将目标点设为鼠标位置
            pro.target_aming.first = pos_mouse.x();
            pro.target_aming.second = pos_mouse.y();
        }
            [[clang::fallthrough]];
        case RotationMode::TowardsTarget: //指向目标
        {
            //根据目标坐标位置计算目标角度
            pro.angular_initial_target.second = qAtan2(pro.target_aming.second - pos.second, pro.target_aming.first - pos.first) * R2D + pro.offset_front;

            //获取当前角度
            pro.rotation.first = p_crt->property.rotation.first;

            //将两个角度转换为区间[0,360]内的等效值
            while (pro.angular_initial_target.second > 360)
                pro.angular_initial_target.second -= 360;
            while (pro.angular_initial_target.second < 0)
                pro.angular_initial_target.second += 360;
            while (pro.rotation.first > 360)
                pro.rotation.first -= 360;
            while (pro.rotation.first < 0)
                pro.rotation.first += 360;

            //获取正向旋转时的差值
            Decimal angle_forward = pro.angular_initial_target.second - pro.rotation.first;
            Decimal angle_backward;
            //获取正向负向的偏移角度
            if (angle_forward > 0)
                angle_backward = 360 - angle_forward;
            else
            {
                angle_backward = -angle_forward;
                angle_forward = 360 - angle_backward;
            }
            //剩余最小角距离
            Decimal offsest_min = angle_forward < angle_backward ? angle_forward : angle_backward;

            bool flag_forward = false;

            if (angle_forward < angle_backward)
            {
                flag_forward = true;
                offsest_min = angle_forward;
            }
            else
                offsest_min = angle_backward;

            //在惯性角度内
            if (offsest_min < pro.rotation.second) //持续减速直到抵达目标
            {
                //计算剩余时间
                auto t = qSqrt(2 * offsest_min / pro.angular_acc_max.second);
                pro.angular_speed_max.first = pro.angular_acc_max.second * t;

                if (offsest_min < pro.angular_acc_max.second) //达到临界值
                {
                    pro.angular_speed_max.first = 0; //设置角速度=0
                }
                if (!flag_forward)
                    pro.angular_speed_max.first = -pro.angular_speed_max.first;
            }
            else //持续加速直到满速
            {
                pro.angular_speed_max.first += pro.angular_acc_max.second;      //根据角加速度更新角速度
                if (pro.angular_speed_max.first > pro.angular_speed_max.second) //限制角速度
                    pro.angular_speed_max.first = pro.angular_speed_max.second;
                else if (pro.angular_speed_max.first < -pro.angular_speed_max.second)
                    pro.angular_speed_max.first = -pro.angular_speed_max.second;
            }
            //            qDebug()<<pro.angular_speed_max.first;
            pro.rotation.first += pro.angular_speed_max.first; //根据角速度更新角度
                                                               //            qDebug()<<pro.rotation.first;
            break;
        }
        case RotationMode::Stop: //自动停止旋转
        {
            if(pro.angular_speed_max.first<pro.angular_acc_max.second||pro.angular_speed_max.first>-pro.angular_acc_max.second)
                pro.angular_speed_max.first=0;
            else if(pro.angular_speed_max.first<0)
                pro.angular_speed_max.first+=pro.angular_acc_max.second;
            else if(pro.angular_speed_max.first>0)
                pro.angular_speed_max.first-=pro.angular_acc_max.second;
            break;
        }
        case RotationMode::Unlimited: //无限制
        {
            pro.angular_speed_max.first += pro.angular_acc_max.first;       //根据角加速度更新角速度
            if (pro.angular_speed_max.first > pro.angular_speed_max.second) //限制角速度
                pro.angular_speed_max.first = pro.angular_speed_max.second;
            else if (pro.angular_speed_max.first < -pro.angular_speed_max.second)
                pro.angular_speed_max.first = -pro.angular_speed_max.second;
            pro.rotation.first += pro.angular_speed_max.first; //更新角度
            break;
        }
        }



    }


//    qDebug()<<QString::asprintf("time_basic:%8lld | time_collision %8lld | time_get_list:%6lld | time_equation %3lld | time_total %8lld | count_collision:%3lld | time_per_collision:%f", time_basic,time_collision,time_get_list,time_equation ,time_total,count,time_collision/double(count));
    ///AI控制
}

void ObjectsControl::process_collision()
{
    QReadLocker locker(&data_runtime.lock);//读锁定
//    QWriteLocker locker(&data_runtime.lock); //写锁定
//    QMutexLocker locker(&data_runtime.mutex);
    auto size=data_runtime.list_objects.size();
    for (int index = 0; index < size; ++index)
    {
        ///坐标计算
        FlyingObject *p_crt = data_runtime.list_objects[index];//当前对象
        auto &pro = p_crt->property;         //物理属性
        auto &acc_p = pro.acceleration_polar;   //加速度_极坐标
        auto &acc_a = pro.acceleration_axis;    //加速度_轴坐标
        auto &v_p = pro.speed_polar;            //速度_极坐标
        auto &v_a = pro.speed_axis;             //加速度_轴坐标
        auto &pos = pro.coordinate;             //位置
        auto &att_v = pro.attenuation_velocity; //速度衰减
        auto &pos_mouse = data_runtime.pos_mouse_scene;//鼠标位置


        ///碰撞检测
        if(!p_crt->property.flag_collision)//没有开启碰撞
            continue;//跳过

        //获取与当前元素发生碰撞的所有元素

        QList<QGraphicsItem*> list;
        list=data_runtime.scene_main->collidingItems(p_crt->item,Qt::IntersectsItemBoundingRect);
//        QList<QGraphicsItem*> list= p_crt->item->collidingItems();

        bool flag_clear_last_collisio=true;//清空上次碰撞的标记

        //遍历每个与当前对象发生碰撞的对象
        for(auto item:list)
        {
            //获取元素的管理对象
            auto p_another=static_cast<Element *>(item)->obj_manage;

            bool flag_continue=false;//跳过标记


            BinaryVector<Decimal> tmp1,tmp2;//临时变量, 注意这两个变量在不同位置含义不同
            Decimal radian{0.0};//轴线弧度(计算结果加上该弧度得到最终结果)
            BinaryVector<Decimal> speed0_r{pro.speed_polar},speed1_r{p_another->property.speed_polar};//两个对象的速度

            //计算相对位置轴坐标
            tmp1.first=p_another->property.coordinate.first-pos.first;
            tmp1.second=p_another->property.coordinate.second-pos.second;

            //计算轴线弧度
            radian = qAtan2(tmp1.second,tmp1.first);

            //检查是否与之前碰撞的对象再次碰撞(消除碰撞粘性)
//            if(p_crt->id_last_collision==p_another->id)
//            {
//                flag_clear_last_collisio=false;
//                flag_continue=true;
//            }
//            if(p_another->id_last_collision==p_crt->id)
//                flag_continue=true;//跳过

//            if(!p_another->property.flag_collision)//查看是否开启碰撞标记(现在会在collideWithItem函数中检测)
//                flag_continue=true;//没开标记跳过

            //都是碰撞对象
            ///处理game数据

            //获取game数据引用
            auto &pro_g_crt = p_crt->property_gaming;
            auto &pro_g_ano = p_another->property_gaming;

            if((pro_g_crt.team==pro_g_ano.team&&(pro_g_crt.flag_team_kill||pro_g_ano.flag_team_kill))||pro_g_crt.team!=pro_g_ano.team)
                //同队伍但开启了友伤 或 不同队伍
            {
                if(!(p_crt->id_penetrating==p_another->id||p_another->id_penetrating==p_crt->id))
                    //穿过目标只计算一次伤害
                {
                    //产生伤害
                    pro_g_crt.endurance.first-=pro_g_ano.damage*pro_g_ano.penetrability/pro_g_crt.resist;
                    pro_g_ano.endurance.first-=pro_g_crt.damage*pro_g_crt.penetrability/pro_g_ano.resist;
                    //穿过物体也算作发生碰撞
                    if(p_crt->property.number_rest_collision>0)
                        --p_crt->property.number_rest_collision;
                    if(p_another->property.number_rest_collision>0)
                        --p_another->property.number_rest_collision;
                }
            }

            //穿透
            if(pro_g_crt.penetrability>pro_g_ano.resist||pro_g_ano.penetrability>pro_g_crt.resist)
            {
                //互相设置穿透id
                p_crt->id_penetrating=p_another->id;
                p_another->id_penetrating=p_crt->id;
                flag_continue=true;
                flag_clear_last_collisio=false;
            }



            //坐标互斥
            if((pro.flag_mutex&&p_another->property.flag_mutex)&&!(p_crt->id_penetrating==p_another->id||p_another->id_penetrating==p_crt->id))
            {
                ///互斥标记开启, 且未发生穿透, 则产生互斥
                //计算互斥后坐标
                tmp1.first=radian;
                tmp1.second=Settings::distance_mutex;
                ToolFunctionsKit::polar_to_axis(tmp1,tmp2);
                //更新互斥后坐标
                p_crt->property.coordinate.first-=tmp2.first;
                p_crt->property.coordinate.second-=tmp2.second;
                p_another->property.coordinate.first+=tmp2.first;
                p_another->property.coordinate.second+=tmp2.second;
            }

            //速度互斥
            if(pro.force_mutex>0||p_another->property.force_mutex>0)
            {
                Decimal force=pro.force_mutex+p_another->property.force_mutex;
                //计算互斥后产生的加速度
                tmp1.first=radian;

                //更新互斥后速度
                tmp1.second=force/pro.mass;
                ToolFunctionsKit::polar_to_axis(tmp1,tmp2);
                pro.speed_axis.first-=tmp2.first;
                pro.speed_axis.second-=tmp2.second;
                ToolFunctionsKit::axis_to_polar(pro.speed_axis,pro.speed_polar);
                //更新互斥后速度
                tmp1.second=force/p_another->property.mass;
                ToolFunctionsKit::polar_to_axis(tmp1,tmp2);
                p_another->property.speed_axis.first+=tmp2.first;
                p_another->property.speed_axis.second+=tmp2.second;
                ToolFunctionsKit::axis_to_polar(p_another->property.speed_axis,p_another->property.speed_polar);
            }


            if(flag_continue)///跳过
                continue;

            //剩余碰撞计数--
            if(p_crt->property.number_rest_collision>0)//不进行判断会产生BUG
                --p_crt->property.number_rest_collision;
            if(p_another->property.number_rest_collision>0)
                --p_another->property.number_rest_collision;

            //绝对弧度转为相对于轴的相对弧度
            speed0_r.first-=radian;
            speed1_r.first-=radian;

            //两个相对极坐标表示速度转轴坐标表示(横坐标为轴向值, 纵坐标为垂直轴向值)
            ToolFunctionsKit::polar_to_axis(speed0_r,tmp1);
            ToolFunctionsKit::polar_to_axis(speed1_r,tmp2);

//            qDebug()<<"<radian> "<<radian;
//            qDebug()<<"<speed_axis> "<<tmp1<<" "<<tmp2;


            auto start2 = std::chrono::steady_clock::now();///------------------------------------------------------------------------------计时开始


            ///轴向发生正碰, 动量守恒, 动能按系数损失(解二元二次方程)
            Decimal m1=pro.mass,m2=p_another->property.mass;// m1, m2
            Decimal x1,x2,d_tmp,c1=0,c2=0, y1,y2;

            //计算常数c1, c2 (减少两次乘法运算优化速度)
            d_tmp=m1*tmp1.first;// m1 * v1
            c1=d_tmp;
            d_tmp*=tmp1.first;// m1 * v1^2
            c2=d_tmp;

            d_tmp=m2*tmp2.first;// m2 * v2
            c1+=d_tmp;
            d_tmp*=tmp2.first;// m2 * v2^2
            c2+=d_tmp;

            //计算最低动能残留
            Decimal eta = (c1*c1)/((m1+m2)*(c2));
            d_tmp=pro.elasticity*p_another->property.elasticity;
            if(eta>d_tmp)
                d_tmp=eta;
            c2*=d_tmp;//考虑动能损失

            //计算两个解
            y1 = 2 * m1 * c1;// y1 临时作 -b
            y2 = 2*m1*(m1+m2);// y2 临时作 2a
            d_tmp = qSqrt(4*m1*(m1*c1*c1-(m1+m2)*(c1*c1-m2*c2)));// (b^2 - 4ac)^0.5
            x1 = (y1 + d_tmp) / y2;//第一个解
            x2 = (y1 - d_tmp) / y2;//第二个解

            //计算两个解对应的另一个速度
            y1 = (c1-m1*x1)/m2;
            y2 = (c1-m1*x2)/m2;


            //以下注释代码含有BUG
/*            if(std::signbit(tmp1.first)==std::signbit(tmp2.first))//速度方向相同, 追击碰撞
            {
                qDebug()<<"追击碰撞";
                if((tmp1.first>tmp2.first&&x1<tmp1.first)||(tmp2.first>tmp1.first&&y1<tmp2.first))
                    //第一组解中, 速度大的那个反而更大了 (等价于小的更小了)
                {
                    x1=x2;//使用第二组解
                    y1=y2;
                }
            }
            else//常规碰撞
            {
                qDebug()<<"常规碰撞";
                //默认使用第一组解
                if(std::signbit(x1)==std::signbit(tmp1.first)&&std::signbit(y1)==std::signbit(tmp2.first))//第一组解同号
                {
                    x1=x2;//使用第二组解
                    y1=y2;
                    qDebug()<<"第二组解";
                }else
                    qDebug()<<"第一组解";
            }*/

            //得到的解更新速度(直接使用第二组解)
            tmp1.first=x2;
            tmp2.first=y2;

            ToolFunctionsKit::axis_to_polar(tmp1,pro.speed_polar);
            ToolFunctionsKit::axis_to_polar(tmp2,p_another->property.speed_polar);

            //更新极坐标速度
            pro.speed_polar.first+=radian;
            p_another->property.speed_polar.first+=radian;


            p_crt->id_last_collision=p_another->id;
            p_another->id_last_collision=p_crt->id;

            ///非轴向动量守恒, 动能按系数转换为转动动能

        }
    }
}

void ObjectsControl::manage_objects()
{
    QWriteLocker locker(&data_runtime.lock); //写锁定
//    QMutexLocker locker(&data_runtime.mutex);

    ///对象派生
    for (int index = 0; index < data_runtime.list_objects.size(); ++index)
    {

        FlyingObject *p_crt = data_runtime.list_objects[index];
        bool flag_destroy=false;//是否需要销毁的标记
        auto &pro = p_crt->property; //获取属性引用

        ///销毁对象

        //检查是否应该删除出界元素
        if (pro.flag_delete_outside_scene)
        {
            auto &pos = pro.coordinate;
            if (pos.first < 0 || pos.first > Settings::width_gaming || pos.second < 0 || pos.second > Settings::height_gaming)
                flag_destroy=true;
        }

        //检查寿命
        if (pro.lifetime == 0)
            flag_destroy=true;
        else if (pro.lifetime > 0)
            --pro.lifetime; //--寿命

        //检查剩余碰撞次数
        if(pro.number_rest_collision==0)
            flag_destroy=true;


        //检查耐久
        if((p_crt)->property_gaming.endurance.first<0)
            flag_destroy=true;

        if(flag_destroy)//销毁对象
        {

            data_runtime.score+=p_crt->property_gaming.score;//计算分数
            //销毁对象
            data_runtime.list_objects.removeAt(index); //删除元素
            if(pro.rule_on_destroy)
                derive_object(pro, data_runtime.pkg.rules_derive[pro.rule_on_destroy]); //使用销毁规则进行派生(亡语)
            p_crt->remove_from_scene();//从场景中删除对象
            if(p_crt==data_runtime.p1)//玩家控制对象
            {
                emit signal_game_over();//发送信号
                continue;//跳过
            }

            delete p_crt->item;                         //释放场景元素
            delete p_crt;                               //释放管理对象
            --index;
            continue;
        }


        ///派生对象
        if (pro.rule<0) //派生规则不存在
            continue;   //跳过

        if (pro.cooldown_drive > 0)
        {
            --pro.cooldown_drive;
            continue;
        }
        else
//            pro.cooldown_drive = pro.rule->period;//重设冷却
             pro.cooldown_drive=data_runtime.pkg.rules_derive[pro.rule].period;

        //! 没有打开标记也应该降低冷却值, 之前这是个BUG
        if (!pro.flag_drive) //派生规则为空, 或标记未打开
            continue;

        derive_object(pro, data_runtime.pkg.rules_derive[pro.rule]);//使用常驻规则进行派生
        ++pro.count_drive; //派生计数+1

    }


    ///场景生成
    //以下代码写得十分混乱
    Integer size=static_cast<Integer>(data_runtime.scene.rules_generate.size());

    if(cooldown>0)
        --cooldown;//降低冷却

    if(size==data_runtime.index_crt_generate_rule)//完成全部生成规则
        return;

    if(cooldown==0)
    {
        if(rest==0)
        {
            ++data_runtime.index_crt_generate_rule;//下一条规则

            if(size>data_runtime.index_crt_generate_rule)
                unit=&data_runtime.scene.rules_generate[static_cast<size_t>(data_runtime.index_crt_generate_rule)];
            else
                return;
            rest=unit->first.number;
        }

        //获取当前规则的引用

        if(data_runtime.score<unit->first.requirement_score)
            return;

        if(rest>=0||(data_runtime.num_updates>=unit->first.requirement_time))
        {
            generate_object(unit->first,unit->second);//生成对象
            cooldown=unit->first.interval;//设置冷却
            --rest;
        }
    }

}

void ObjectsControl::update_property()
{
//    QWriteLocker locker(&data_runtime.lock); //写锁定
    QReadLocker locker(&data_runtime.lock); //读锁定
//    QMutexLocker locker(&data_runtime.mutex);


    for (auto object : data_runtime.list_objects)
    {
        object->item->setPos(object->property.coordinate.first, object->property.coordinate.second); //更新位置
        object->item->setRotation(object->property.rotation.first);                                  //更新角度

        Element* p = static_cast<Element*>(object->item);//下一帧
        if(p->number_frame>1)
            p->next_frame();
        continue;
    }
}

void ObjectsControl::reset()
{
    this->unit=nullptr;
    this->cooldown=this->rest=0;
}

void ObjectsControl::derive_object(const ObjectControlProperty &pro,const DeriveRule &rule)
{
    FlyingObject *obj_new{nullptr};

    //逐个遍历派生单元
    for (const DeriveUnit &unit : rule.units)
    {
        if(ToolFunctionsKit::get_random_decimal_0_1()>unit.probability)
            continue;

        obj_new = new FlyingObject(*(unit.object)); //申请新对象

        obj_new->property.lifetime*=1 + (ToolFunctionsKit::get_random_decimal_0_1() - 0.5)*unit.float_lifetime*2;

        ///处理方向

        //方向参照
        switch (unit.ref_direction)
        {
        case DR::RelativeToParentDirection: //相对基对象
        {
            obj_new->property.rotation.first = pro.rotation.first - pro.offset_front;
            break;
        }
        case DR::RelativeToParentSpeed: //相对基对象速度
        {
            obj_new->property.rotation.first += pro.speed_polar.first * R2D;
            break;
        }
        case DR::RelativeToParentAcc: //相对基对象加速度
        {
            obj_new->property.rotation.first += pro.acceleration_polar.first * R2D;
            break;
        }
        case DR::Absolute: //绝对方向
            break;
        }
        //方向浮动
        obj_new->property.rotation.first+=(ToolFunctionsKit::get_random_decimal_0_1() - 0.5) * 360 * unit.float_direction;

        BinaryVector<Decimal> bv_tmp{}; //临时变量(极坐标)

        ///处理坐标

        obj_new->property.coordinate = unit.position;
        if (unit.flag_relative_position) //相对基对象坐标
        {
            //先将相对的放置坐标转为极坐标
            ToolFunctionsKit::axis_to_polar(unit.position, bv_tmp);
            bv_tmp.first += (obj_new->property.rotation.first + pro.offset_front) * D2R;
            ToolFunctionsKit::polar_to_axis(bv_tmp, obj_new->property.coordinate);

            obj_new->property.coordinate.first += pro.coordinate.first;
            obj_new->property.coordinate.second += pro.coordinate.second;
        }


        ///处理速度

        bv_tmp = unit.speed;//获取规则指定的速度
        //速度方向参照
        switch (unit.ref_speed_direction)
        {
        case DR::RelativeToParentDirection: //相对基对象
        {
            bv_tmp.first = pro.rotation.first - pro.offset_front;
            break;
        }
        case DR::RelativeToParentSpeed: //相对基对象速度
        {
            bv_tmp.first += pro.speed_polar.first * R2D;
            break;
        }
        case DR::RelativeToParentAcc: //相对基对象加速度
        {
            bv_tmp.first += pro.acceleration_polar.first * R2D;
            break;
        }
        case DR::Absolute: //绝对方向
            break;
        }

        if (unit.speed.second < 0)                              //如果速度大小为负
            bv_tmp.second = unit.object->property.velocity_max; //使用最大速率

        //速度方向浮动
        bv_tmp.first+=(ToolFunctionsKit::get_random_decimal_0_1() - 0.5) * 360 * unit.float_direction_speed;
        bv_tmp.first*=D2R;//转弧度
        //速度大小浮动
        bv_tmp.second*=1 + (ToolFunctionsKit::get_random_decimal_0_1() - 0.5)*unit.float_magnitude_speed*2;

        ToolFunctionsKit::polar_to_axis(bv_tmp, obj_new->property.speed_axis); //转为轴坐标

        if (unit.flag_inherit_speed) //速度继承
        {
            //继承全局速度
            obj_new->property.speed_axis.first += pro.speed_axis.first;
            obj_new->property.speed_axis.second += pro.speed_axis.second;
        }
        //根据轴坐标转换为极坐标
        ToolFunctionsKit::axis_to_polar(obj_new->property.speed_axis, obj_new->property.speed_polar);


        ///处理加速度
        bv_tmp = unit.acceleration;
        //加速度方向参照
        switch (unit.ref_speed_direction)
        {
        case DR::RelativeToParentDirection: //相对基对象
        {
            bv_tmp.first = pro.rotation.first - pro.offset_front;
            break;
        }
        case DR::RelativeToParentSpeed: //相对基对象速度
        {
            bv_tmp.first += pro.speed_polar.first * R2D;
            break;
        }
        case DR::RelativeToParentAcc: //相对基对象加速度
        {
            bv_tmp.first += pro.acceleration_polar.first * R2D;
            break;
        }
        case DR::Absolute: //绝对方向
            break;
        }
        if (unit.acceleration.second < 0)                      //如果速度大小为负
            bv_tmp.second = unit.object->property.acceleration_max; //使用最大加速度
        //加速度方向浮动
        bv_tmp.first+=(ToolFunctionsKit::get_random_decimal_0_1() - 0.5) * 360 * unit.float_direction_acc;
        bv_tmp.first*=D2R;//转弧度
        //加速度大小浮动
        bv_tmp.second*=1 + (ToolFunctionsKit::get_random_decimal_0_1() + 0.5)*unit.float_magnitude_acc*2;

        ToolFunctionsKit::polar_to_axis(bv_tmp, obj_new->property.acceleration_axis); //转为轴坐标

        //根据轴坐标转换为极坐标
        ToolFunctionsKit::axis_to_polar(obj_new->property.acceleration_axis, obj_new->property.acceleration_polar);

        data_runtime.list_objects.push_back(obj_new); //添加到管理列表
        obj_new->add_to_scene(data_runtime.scene_main);//添加到场景
    }
}

void ObjectsControl::generate_object(const SceneGenerateRule &rule, const ObjectActionProperty &pro_a)
{
    if(data_runtime.pkg.objects.find(rule.name_object)==data_runtime.pkg.objects.end())
        throw QString::asprintf("<ERROR> cannot find object %s in pkg %s",rule.name_object.toStdString().c_str(),data_runtime.pkg.path_pkg.toStdString().c_str());
    auto &obj_target=data_runtime.pkg.objects[rule.name_object];//获取目标对象引用

    FlyingObject * obj_new=new FlyingObject(obj_target);//复制构造

    //处理坐标
    if(rule.position.x()<0)//横坐标小于0, 随机放置
    {
        //随机横坐标
        obj_new->property.coordinate.first=ToolFunctionsKit::get_random_decimal_0_1()*data_runtime.scene_main->width();

        //随机纵坐标
        if(!rule.flag_in_viewport
                &&obj_new->property.coordinate.first>data_runtime.scene_main->rect_exposed.left()
                &&obj_new->property.coordinate.first<data_runtime.scene_main->rect_exposed.right())//视图内不可放置
        {
            //纵坐标可能在视图内
            if(data_runtime.scene_main->rect_exposed.top()<0)//顶部无间隙
            {
                obj_new->property.coordinate.second=
                        ToolFunctionsKit::get_random_decimal_0_1()
                        *(data_runtime.scene_main->height()-data_runtime.scene_main->rect_exposed.bottom())
                        +data_runtime.scene_main->rect_exposed.bottom();
            }
            else if(data_runtime.scene_main->rect_exposed.bottom()>data_runtime.scene_main->height())//底部无间隙
            {
                obj_new->property.coordinate.second=
                        ToolFunctionsKit::get_random_decimal_0_1()
                        *(data_runtime.scene_main->height()-data_runtime.scene_main->rect_exposed.top());
            }
            else//上下都有间隙
            {
                if(ToolFunctionsKit::get_random_decimal_0_1()<0.5)//选择上间隙
                {
                    obj_new->property.coordinate.second=
                            ToolFunctionsKit::get_random_decimal_0_1()
                            *(data_runtime.scene_main->height()-data_runtime.scene_main->rect_exposed.top());
                }
                else//选择下间隙
                {
                    obj_new->property.coordinate.second=
                            ToolFunctionsKit::get_random_decimal_0_1()
                            *(data_runtime.scene_main->height()-data_runtime.scene_main->rect_exposed.bottom())
                            +data_runtime.scene_main->rect_exposed.bottom();
                }

            }
        }
        else//视图内可以放置或横坐标不在范围内
            obj_new->property.coordinate.second=ToolFunctionsKit::get_random_decimal_0_1()*data_runtime.scene.size.ry();
    }
    else//固定放置
    {
        obj_new->property.coordinate.first=rule.position.x();
        obj_new->property.coordinate.second=rule.position.y();
    }

    //处理旋转(方向)
    if(rule.rotation<0)
        obj_new->property.rotation.first=360*ToolFunctionsKit::get_random_decimal_0_1();//随机旋转
    else
        obj_new->property.rotation.first=rule.rotation;

    obj_new->property_action=pro_a;//设置行为属性

    if(pro_a.flag_player_manip)//player操纵
        data_runtime.p1=obj_new;//设置

    data_runtime.list_objects.push_back(obj_new); //添加到管理列表
    obj_new->add_to_scene(data_runtime.scene_main);//添加到场景
}



GameWidget::GameWidget(MainWindow *_main_window)
    : QWidget(), main_window(_main_window)
{
    init_components();
    init_UI();
    init_threads();
    init_signal_slots();

    load_title_images();//加载标题背景图片
}

GameWidget::~GameWidget()
{

    thread_data_process.quit(); //退出线程
//    thread_objects_management.quit();
    thread_data_process.wait(); //等待线程结束
//    thread_objects_management.wait();
    this->reset();//重置
}

void GameWidget::init_components()
{
    layout_main = new QGridLayout();
    //scene_main并不是字段, 是全局变量
    data_runtime.scene_main = new GraphicsScene(0, 0, Settings::width_gaming, Settings::height_gaming);
    data_runtime.view_main = new GraphicsView(data_runtime.scene_main);

    //标题页场景
    scene_title=new GraphicsScene(0,0,Settings::width_gaming, Settings::height_gaming);
    view_title=new GraphicsView(scene_title);

    widget_main=new QStackedWidget();
    widget_title=new QWidget();
    panel_title=new QWidget();
    widget_start=new QWidget();
    panel_start=new QWidget();
    list_widget_start=new QListWidget();
    widget_log=new QWidget();
    widget_menu = new QWidget();
    panel_log=new QWidget();
    widget_game_info=new QWidget();
    panel_game_over=new QWidget();

    layout_widget_menu = new QGridLayout();
    layout_title=new QGridLayout();
    layout_start=new QGridLayout();
    layout_log=new QGridLayout();
    layout_panel_title=new QGridLayout();
    layout_panel_start=new QGridLayout();
    layout_panel_log=new QGridLayout();
    layout_widget_game_info=new QGridLayout();

    button_start=new Button("START");
    button_load=new Button("LOAD");
    button_options=new Button("OPTIONS");
    button_exit=new Button("EXIT");
    button_refresh_start=new Button("REFRESH");

    button_resume_pause_menu = new Button("RESUME");
    button_exit_pause_menu = new Button("EXIT");
    button_to_info_page_pause_menu= new Button("LOG");
    button_back_start=new Button("BACK");
    button_play_start=new Button("PLAY!");
    button_close_log=new Button("CLOSE");

    label_info_esc_menu = new QLabel();
    label_title=new QLabel();
    label_bottom_info_title=new QLabel();
    label_start_page_top=new QLabel();
    label_info_page_top=new QLabel();
    label_game_info=new QLabel();
    label_game_over=new QLabel();

    browser_info=new QTextBrowser();
}

void GameWidget::init_UI()
{

    ///总体样式
    this->setStyleSheet(
        //全部
        "QWidget { background-color:rgba(0,0,0,0.0); color: #ffffff; font-size: 20px; font-family: consolas; }"
        //暂停菜单
        "QWidget#widget_menu { background-color: rgba(0,0,0,0.6); }"
        "QWidget#widget_game_info { font-size: 30px; background-color: rgba(0,0,0,0.0); }"
        "QWidget#widget_game_info > QLabel { font-size: 30px; background-color: rgba(0,0,0,0.9); }"
        //面板
        "QWidget#panel { background-color: rgba(0,0,0,0.8); }"
        //按钮
        "QPushButton { background-color: rgba(0,0,0,1.0); color: #ffffff; border: 2px solid #00ff00; width: 200px; height:50px; }"
        "QWidget:disabled { background-color: rgba(0,0,0,0.0); color: #999999; border: 2px solid #000000; }"
        //按钮hover
        "QPushButton::hover{ background-color: rgba(200,200,200,1.0); border: 2px solid #000000; }"
        //面板下
        "QWidget#panel > QListWidget { border: 2px solid #00ff00; }"
        "QWidget#panel > QListWidget::item { border: 2px rgba(0,0,0,0.0); border-bottom: 2px solid #555555; margin: 10px; padding: 10px; }"
        "QWidget#panel > QListWidget::item:selected { color: #ffffff; background-color: rgba(0,0,0,0.8); border-bottom: 2px solid #00ff00; }"
        "QWidget#panel > QTextBrowser { border: 2px solid #00ff00; }"
        "QWidget#panel > QPushButton { width: 400px; }"
        "QWidget#panel > QPushButton#refresh { width:200px; border: 2px solid #000000; }"
        //label背景色
        "QWidget QLabel#title{ background-color: rgba(0,0,0,0.8); color: #ffffff; padding:10px; }"
    );


    ///主窗口
    this->setWindowTitle("BarrageSimulator - Particles");

    this->setMinimumSize(Settings::width_gaming,Settings::height_gaming);

    this->setLayout(layout_main);
    layout_main->setContentsMargins(0,0,0,0);

    layout_main->addWidget(widget_main);


    ///标题页
    widget_title->setLayout(layout_title);//设置布局
    layout_title->setContentsMargins(0,0,0,0);


    //模糊效果
    effect_blur.setBlurRadius(Settings::radius_blur);
    effect_blur.setBlurHints(QGraphicsBlurEffect::BlurHint::PerformanceHint);
    bg.setGraphicsEffect(&effect_blur);

    auto widget_OGL =new QOpenGLWidget();

    view_title->setViewport(widget_OGL);
//    view_title->setGraphicsEffect(&effect_blur);

    view_title->setSceneRect(0,0,view_title->width(),view_title->height());

    view_title->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); //水平
    view_title->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   //垂直
    view_title->setParent(this);
    view_title->lower();

    view_title->setCacheMode(QGraphicsView::CacheBackground);
//    view_title->setRenderHints(QPainter::Antialiasing);//抗锯齿

    label_title->setPixmap(QPixmap(Settings::path_title_image));

    layout_title->addWidget(label_title,0,0,1,3,Qt::AlignLeft|Qt::AlignTop);
    layout_title->setRowStretch(1,1);
    layout_title->addWidget(panel_title,2,0,1,4);
    layout_title->setRowStretch(3,1);
    layout_title->addWidget(label_bottom_info_title,4,0,1,4,Qt::AlignLeft);

    panel_title->setLayout(layout_panel_title);
    panel_title->setObjectName("panel");

    layout_panel_title->addWidget(button_start,0,1,1,2,Qt::AlignCenter);
    layout_panel_title->addWidget(button_load,1,1,1,2,Qt::AlignCenter);
    layout_panel_title->addWidget(button_options,2,1,1,2,Qt::AlignCenter);
    layout_panel_title->addWidget(button_exit,3,1,1,2,Qt::AlignCenter);

    widget_main->insertWidget(TitlePage,widget_title);


    ///start页
    widget_start->setLayout(layout_start);
    layout_start->setContentsMargins(0,0,0,0);

    label_start_page_top->setText("Select Level");
    label_start_page_top->setObjectName("title");

    layout_start->addWidget(label_start_page_top,0,0,Qt::AlignLeft);
    layout_start->setRowStretch(1,1);
    layout_start->addWidget(panel_start,2,0);
    layout_start->setRowStretch(3,1);

    panel_start->setLayout(layout_panel_start);
    panel_start->setObjectName("panel");

    button_refresh_start->setObjectName("refresh");
    button_play_start->setEnabled(false);//默认不可用

    layout_panel_start->addWidget(list_widget_start,0,1,Qt::AlignCenter);//列表组件
    layout_panel_start->addWidget(button_refresh_start,0,2,Qt::AlignLeft|Qt::AlignTop);//刷新按钮
    layout_panel_start->addWidget(button_play_start,1,2,Qt::AlignLeft);//刷新按钮
    layout_panel_start->addWidget(button_back_start,1,1,Qt::AlignCenter);//返回按钮

    layout_panel_start->setColumnStretch(0,1);
    layout_panel_start->setColumnStretch(1,0);
    layout_panel_start->setColumnStretch(2,1);

    list_widget_start->setFixedSize(402,500);
    list_widget_start->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);//只能单选
    list_widget_start->setFocusPolicy(Qt::NoFocus);//不聚焦
    model_selection=list_widget_start->selectionModel();//获取选择集模型

    widget_main->insertWidget(StartPage,widget_start);

    ///log页
    widget_log->setLayout(layout_log);
    layout_log->setContentsMargins(0,0,0,0);

    label_info_page_top->setText("LOG");
    label_info_page_top->setObjectName("title");
    layout_log->addWidget(label_info_page_top,0,0,Qt::AlignLeft);
    layout_log->addWidget(panel_log,2,0);//面板
    layout_log->setRowStretch(0,0);
    layout_log->setRowStretch(1,1);
    layout_log->setRowStretch(2,5);
    layout_log->setRowStretch(3,1);

    panel_log->setLayout(layout_panel_log);
    panel_log->setObjectName("panel");

    browser_info->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    layout_panel_log->addWidget(browser_info,0,0);
    layout_panel_log->addWidget(button_close_log,1,0,Qt::AlignRight);

    widget_main->insertWidget(LogPage,widget_log);


    ///game页
    auto p =new QOpenGLWidget();

    data_runtime.scene_main->setItemIndexMethod(QGraphicsScene::ItemIndexMethod::NoIndex);//设置底层索引
    data_runtime.view_main->setViewport(p);
    data_runtime.view_main->setMouseTracking(true);

    data_runtime.view_main->setContentsMargins(0,0,0,0);
    data_runtime.view_main->setSceneRect(0,0,data_runtime.view_main->width(),data_runtime.view_main->height());

    //关闭滚动条
    data_runtime.view_main->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); //水平
    data_runtime.view_main->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   //垂直

//    this->setLayout(layout_gird_main);
//    layout_gird_main->setContentsMargins(2, 2, 2, 2);
//    layout_gird_main->addWidget(widget_main);

    widget_main->insertWidget(GamePage,data_runtime.view_main);

    //主界面设置


    ///menu页
    widget_menu->setLayout(layout_widget_menu);

    layout_widget_menu->addWidget(label_info_esc_menu, 0, 0, Qt::AlignTop | Qt::AlignLeft);
    layout_widget_menu->setRowStretch(1, 10);
    layout_widget_menu->addWidget(button_resume_pause_menu, 2, 0, Qt::AlignLeft);
    layout_widget_menu->addWidget(button_to_info_page_pause_menu, 3, 0, Qt::AlignLeft);
    layout_widget_menu->addWidget(button_exit_pause_menu, 4, 0, Qt::AlignLeft);

    label_info_esc_menu->setText("Barrage Simulator");
    label_info_esc_menu->setObjectName("title");

    widget_menu->setParent(this);
    widget_menu->setVisible(false); //不可见
    widget_menu->raise();           //置于顶层
    widget_menu->setObjectName("widget_menu");

    ///game_info页
    widget_game_info->setLayout(layout_widget_game_info);

    layout_widget_game_info->setSpacing(0);


    layout_widget_game_info->addWidget(label_game_info,0,0,Qt::AlignLeft);
    layout_widget_game_info->addWidget(panel_game_over,2,0,1,4,Qt::AlignCenter);
    layout_widget_game_info->setRowStretch(0,0);
    layout_widget_game_info->setRowStretch(1,1);
    layout_widget_game_info->setRowStretch(2,1);
    layout_widget_game_info->setRowStretch(3,1);

    panel_game_over->setVisible(false);//不可见
    widget_game_info->setAttribute(Qt::WA_TransparentForMouseEvents,true);//不响应鼠标

    widget_game_info->setVisible(false);
    widget_game_info->setParent(this);
    widget_game_info->raise();
    widget_game_info->setObjectName("widget_game_info");


    //各种组件的尺寸初始设置
    this->resize(Settings::width_gaming,Settings::height_gaming);

    view_title->resize(Settings::width_gaming,Settings::height_gaming);

    widget_menu->resize(Settings::width_gaming,Settings::height_gaming);

    widget_main->setCurrentIndex(0);
}

void GameWidget::init_signal_slots()
{
    //定时器更新信号
    connect(&timer, &QTimer::timeout, this, &GameWidget::update);
    connect(&timer_title, &QTimer::timeout, this, &GameWidget::update_bg_image_position);

    //消息推送
    connect(object_thread_data_process, &ObjectsControl::signal_push_info, this, &GameWidget::push_info);
    connect(this, &GameWidget::signal_push_info, main_window, &MainWindow::push_info);
    connect(this, &GameWidget::signal_push_info, this, &GameWidget::push_info);

    //按钮
    connect(button_resume_pause_menu, &QPushButton::clicked, this, &GameWidget::esc);
    connect(button_to_info_page_pause_menu, &QPushButton::clicked, this, &GameWidget::goto_log_page);
    connect(button_close_log, &QPushButton::clicked, this, &GameWidget::close_log_page);
    connect(button_start, &QPushButton::clicked, this, &GameWidget::goto_start_page);
    connect(button_back_start, &QPushButton::clicked, this, &GameWidget::goto_title_page);
    connect(button_refresh_start, &QPushButton::clicked, this, &GameWidget::load_scene_list);
    connect(model_selection, &QItemSelectionModel::selectionChanged,this,&GameWidget::handle_select);
    connect(button_play_start, &QPushButton::clicked,this,&GameWidget::load_scene);

    connect(button_exit_pause_menu, &QPushButton::clicked,this,&GameWidget::exit);

    connect(&control, &ObjectsControl::signal_game_over,this,&GameWidget::game_over);
}

void GameWidget::init_threads()
{
    object_thread_data_process = new ObjectsControl();              //new一个线程对象
    object_thread_data_process->moveToThread(&thread_data_process); //转移至线程
    thread_data_process.start();                                    //开启线程

    //    object_thread_objects_management = new ObjectsControl();                    //new一个线程对象
    //    object_thread_objects_management->moveToThread(&thread_objects_management); //转移至线程
    //    thread_objects_management.start();                                          //开启线程

    ///连接线程信号

    //线程中的对象稍后释放
    connect(&thread_data_process, &QThread::finished, object_thread_data_process, &QObject::deleteLater);
    //    connect(&thread_objects_management, &QThread::finished, object_thread_objects_management, &QObject::deleteLater);
    //耗时操作
    connect(this, &GameWidget::signal_process_data, object_thread_data_process, &ObjectsControl::process_data);
    //    connect(this, &GamingWidget::signal_manage_objects, object_thread_objects_management, &ObjectsControl::manage_objects);
}

void GameWidget::load_title_images()
{
    QDir dir(Settings::path_dir_title_bg);
    QStringList filter{"*.png","*.jpg"};
    QFileInfoList infos=dir.entryInfoList(filter,QDir::Files|QDir::Readable,QDir::Name);

    if(!dir.exists()||infos.size()==0)
    {
        this->label_bottom_info_title->setText("<WARN> No background images found in DIR "+Settings::path_dir_title_bg);
        emit signal_push_info("<WARN> No background images found in DIR "+Settings::path_sounds);
        return;
    }

    QPixmap pixmap;

    //读取全部图片并放入容器
    for(QFileInfo info:infos)
    {
        if(!pixmap.load(info.filePath()))
            continue;//读取失败则跳过
        images_title.append(pixmap);
    }

    timer_title.setInterval(Settings::interval_title);
//    timer_title.start();

    scene_title->addItem(&bg);//添加到场景
}

void GameWidget::load_audio_files()
{
    QDir dir(Settings::path_sounds);
    QStringList filter{"*.wav"};

    QFileInfoList infos=dir.entryInfoList(filter,QDir::Files|QDir::Readable,QDir::Name);

    if(!dir.exists()||infos.size()==0)
    {
        this->label_bottom_info_title->setText("<WARN> No sound files found in DIR "+Settings::path_sounds);
        emit signal_push_info("<WARN> No sound files found in DIR "+Settings::path_sounds);
        return;
    }

    QSoundEffect effect;

    //读取全部音频文件并放入容器
    for(QFileInfo info:infos)
    {
        effect.setSource(info.filePath());
        sound_effects.insert(info.fileName(),effect);//添加到容器
    }

//    if(sound_effects.find(Settings:))


}

void GameWidget::key_process()
{
    if(widget_main->currentIndex()==GamePage&&this->status==Status::Running&&data_runtime.p1!=nullptr)//检查状态
    {
        data_runtime.p1->property.mode_movement = MovementMode::Unlimited; //无限制运动

        ///鼠标控制
        if (data_runtime.status_keys[Key::ML] || mouse_delay > 0) //鼠标左键
            data_runtime.p1->property.flag_drive = true;          //派生状态打开
        else
            data_runtime.p1->property.flag_drive = false; //派生状态关闭

        ///键盘控制
        //方向控制
        if (data_runtime.status_keys[Key::P0_UP] && !data_runtime.status_keys[Key::P0_DOWN]) //按住上键没有按住下键
        {
            if (data_runtime.status_keys[Key::P0_LEFT]) //左键
            {
                //左上
                data_runtime.p1->property.acceleration_polar.first = -PI * 3 / 4; //135度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else if (data_runtime.status_keys[Key::P0_RIGHT]) //左键
            {
                //右上
                data_runtime.p1->property.acceleration_polar.first = -PI / 4; //45度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else
            {
                //上
                data_runtime.p1->property.acceleration_polar.first = -PI / 2; //90度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
        }
        else if (data_runtime.status_keys[Key::P0_DOWN] && !data_runtime.status_keys[Key::P0_UP]) //按住上键没有按住下键
        {
            if (data_runtime.status_keys[Key::P0_LEFT]) //左键
            {
                //左下
                data_runtime.p1->property.acceleration_polar.first = PI * 3 / 4; //135度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else if (data_runtime.status_keys[Key::P0_RIGHT]) //左键
            {
                //右下
                data_runtime.p1->property.acceleration_polar.first = PI / 4; //45度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else
            {
                //下
                data_runtime.p1->property.acceleration_polar.first = PI / 2; //90度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
        }
        else if (data_runtime.status_keys[Key::P0_LEFT] && !data_runtime.status_keys[Key::P0_RIGHT]) //按住左键没有按住右键
        {
            if (data_runtime.status_keys[Key::P0_UP]) //上键
            {
                //左上
                data_runtime.p1->property.acceleration_polar.first = -PI * 3 / 4; //135度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else if (data_runtime.status_keys[Key::P0_DOWN]) //下键
            {
                //左下
                data_runtime.p1->property.acceleration_polar.first = PI * 3 / 4; //45度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else
            {
                //左
                data_runtime.p1->property.acceleration_polar.first = PI; //90度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
        }
        else if (data_runtime.status_keys[Key::P0_RIGHT] && !data_runtime.status_keys[Key::P0_LEFT]) //按住左键没有按住右键
        {
            if (data_runtime.status_keys[Key::P0_UP]) //上键
            {
                //右上
                data_runtime.p1->property.acceleration_polar.first = -PI * 1 / 4; //45度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else if (data_runtime.status_keys[Key::P0_DOWN]) //下键
            {
                //右下
                data_runtime.p1->property.acceleration_polar.first = PI * 1 / 4; //45度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
            else
            {
                //右
                data_runtime.p1->property.acceleration_polar.first = 0; //0度
                data_runtime.p1->property.acceleration_polar.second = data_runtime.p1->property.acceleration_max;
            }
        }
        else
        {
            data_runtime.p1->property.mode_movement = MovementMode::Stop; //自动停止
        }

    }

    //退出键
    if (data_runtime.status_keys[Key::ESC])
    {
        data_runtime.status_keys[Key::ESC] = false; //重置按键状态
        esc();                                      //退出键
    }
}

void GameWidget::esc()
{
    static bool active = false;

    if (active) //当前处于暂停状态
    {
        if(this->widget_main->currentIndex()==GamePage)
        {
            this->timer.start(); //开启计时器
            this->status=Status::Running;//状态设为运行(继续运行)
        }

        this->widget_menu->setVisible(false);
    }
    else
    {
        if(this->widget_main->currentIndex()==GamePage)
        {
            this->timer.stop(); //停止计时器
            this->status=Status::Pause;//状态设为暂停
        }
        this->widget_menu->setVisible(true);
    }
    active = !active;
}

void GameWidget::goto_page(GameWidget::Page page)
{
    emit signal_push_info(QString::asprintf("<function called> GameWidget::goto_page(%d)",page));
    page_last=static_cast<Page>(widget_main->currentIndex());
    widget_main->setCurrentIndex(page);
}

void GameWidget::clear()
{
    emit signal_push_info("<function called> GameWidget::clear()");
    data_runtime.clear();
}

void GameWidget::load_scene()
{
    emit signal_push_info("<function called> GameWidget::load_scene()");

    //获取选中的文件名
    QString path_scene_file=path_scene_files.at(model_selection->selectedRows()[0].row());
    path_scene_file=Settings::path_scenes+path_scene_file;

    try
    {
        data_runtime.scene.set_file_path(path_scene_file);//设置场景路径
        data_runtime.scene.load();//加载场景文件

        data_runtime.pkg.set_package_path(Settings::path_resource_pkg+data_runtime.scene.name_resource);
        data_runtime.pkg.load();//加载资源文件

        data_runtime.scene_main->setSceneRect(0,0,data_runtime.scene.size.rx(),data_runtime.scene.size.ry());
        const auto &rect = data_runtime.scene_main->sceneRect();
        data_runtime.view_main->setSceneRect(-rect.right(),-rect.bottom(),3*rect.width(),3*rect.height());

        if(data_runtime.scene.image_internal.size()>0)
        {
            //设置笔刷
            data_runtime.scene_main->brush_internal.setTexture(data_runtime.pkg.pixmaps[data_runtime.scene.image_internal]);
        }

        if(data_runtime.scene.image_external.size()>0)
        {
            //设置笔刷
            data_runtime.scene_main->brush_external.setTexture(data_runtime.pkg.pixmaps[data_runtime.scene.image_external]);
        }

        goto_page(GamePage);

        this->view_title->setVisible(false);
        this->timer_title.stop();//关闭主界面
        this->status=Status::Running;
        this->timer.start(Settings::interval);
        this->widget_game_info->setVisible(true);
    }
    catch (const QString &e)
    {
        qDebug()<<e;
        emit signal_push_info(e);
    }
}

void GameWidget::load_scene_list()
{
    emit signal_push_info("<function called> GameWidget::load_scene_list()");

    list_widget_start->clear();

    QDir dir(Settings::path_scenes);
    if(!dir.exists())
    {
        list_widget_start->clear();//清空
        return;
    }
    QStringList filter{"*.json"};
    //获取文件路径
    path_scene_files=dir.entryList(filter,QDir::Readable|QDir::Files,QDir::Name);

    list_widget_start->addItems(path_scene_files);

}

void GameWidget::handle_select()
{
    auto rows=model_selection->selectedRows();

    if(rows.size()==1)
        this->button_play_start->setEnabled(true);
    else
        this->button_play_start->setEnabled(false);
}

void GameWidget::initialize()
{
    timer.setTimerType(Qt::TimerType::PreciseTimer);//精密计时器
    timer.start(Settings::interval); //开启定时器

}

void GameWidget::reset()
{
    this->status=Status::Over;
    this->widget_game_info->setVisible(false);
    timer.stop();//结束定时器
    timer_title.start();
    this->view_title->setVisible(true);
    this->clear();
    goto_start_page();
    control.reset();
    this->cooldown_next_data_update=0;
}

void GameWidget::start()
{
    this->show();
    this->timer_title.start();//标题更新计时器开启
    Settings::reset_key_map();//重置键位
}

void GameWidget::exec()
{
    this->show();
    QEventLoop loop;
    loop.exec();
}

void GameWidget::test()
{
//    data_runtime.p1 = new FlyingObject(objects_inner[InnerObjects::Block_zero_blue]);
//    data_runtime.p1->property.coordinate = {100, 100};
//    data_runtime.p1->add_to_scene(scene_main);    //添加到场景
//    data_runtime.list_objects << data_runtime.p1; //添加到对象列表中

//    auto p = new FlyingObject(objects_inner[InnerObjects::Block_zero_red]);
//    p->property.coordinate={500,200};
//    p->add_to_scene(scene_main);    //添加到场景
//    data_runtime.list_objects << p; //添加到对象列表中

//    initialize();//初始化

//    this->setVisible(true);

//    Settings::reset_key_map(); //重置/初始化键位

//    this->widget_main->setCurrentIndex(GamePage);



    try
    {
        ResourcePackage pkg;
        pkg.set_package_path("./data/resources/default");
        pkg.load();//加载包
    }
    catch (const QString & e)
    {
        qDebug()<<e;
    }


}

void GameWidget::update()
{
    //状态栏数据更新
    static auto start = std::chrono::steady_clock::now();
    auto end = start;
    start=std::chrono::steady_clock::now();
    auto tt = std::chrono::duration_cast<std::chrono::microseconds>(start-end).count();
    time_consumption_total+=tt;

    if(data_runtime.num_updates%Settings::count_frames==0)
    {
        info_status_bar.time_consumption_average=time_consumption_total/Settings::count_frames;//计算平均时长
        time_consumption_total=0;
    }

    info_status_bar.time_consumption=tt;

    info_status_bar.pos_mouse = data_runtime.view_main->pos_mouse;    //鼠标坐标
    info_status_bar.num_updates =data_runtime.num_updates;            //更新数
    info_status_bar.num_objects = data_runtime.list_objects.size();   //对象数量
    data_runtime.pos_mouse_scene = data_runtime.view_main->pos_mouse; //鼠标坐标

    emit signal_send_status_bar_info(&this->info_status_bar); //更新状态栏信息

    if (cooldown_next_data_update == 0)
    {
//        emit signal_process_data(); //发送信号, 子线程处理数据
        if(this->mouse_event_not_handled)
            mouse_event_not_handled=false;
        try
        {
//            emit signal_process_data();  //发送信号, 子线程处理数据

            control.process_data();      //数据处理
            auto start = std::chrono::steady_clock::now();
            control.process_collision(); //处理碰撞
            auto end = std::chrono::steady_clock::now();
            control.manage_objects();    //管理对象
            control.update_property();   //更新元素属性


            auto tt = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
            qDebug()<<tt;
        }
        catch(const QString &e)
        {
            emit signal_push_info(e);
            this->status=Status::Over;//运行结束
        }
        cooldown_next_data_update = Settings::period_data_update;

        if (mouse_delay > 0)
            --mouse_delay;

        if(data_runtime.p1->item)
            data_runtime.view_main->centerOn(data_runtime.p1->item);//保持中心

        this->label_game_info->setText(
        QString::asprintf(
                        "HP: %3.0f / %-5.0f   \nScore: %-5lld",
                        data_runtime.p1->property_gaming.endurance.first,
                        data_runtime.p1->property_gaming.endurance.second,
                        data_runtime.score
                        ));
    }
    key_process(); //按键处理

    --cooldown_next_data_update;

//    data_runtime.view_main->update(); //视图更新
//    data_runtime.view_main->viewport()->update();
//    view_main->repaint();
    ++data_runtime.num_updates;
}

void GameWidget::update_bg_image_position()
{
    static Integer count=0;//刷新次数
    static int index_crt=0;
    static int size=this->images_title.size();//获取图片数量
    static QSize size_pixmap_crt;

    if(size<=0)
        return;//没有图片直接退出

    //每秒更新数
//    const static Integer count_per_second = 1000/Settings::interval_title;

    auto pos = bg.pos();
    pos.rx()+=speed_bg_moving.first;
    pos.ry()+=speed_bg_moving.second;
    view_title->update();

    if(size_pixmap_crt.rheight()>view_title->height()&&size_pixmap_crt.rwidth()>view_title->width())
    {
        Decimal tmp{0.0};
        tmp=-(size_pixmap_crt.rwidth()-view_title->width())/2;
        if(pos.rx()<tmp)
        {
            pos.rx()=tmp;
            speed_bg_moving.first=-speed_bg_moving.first;
        }
        tmp=-tmp;
        if(pos.rx()>tmp)
        {
            pos.rx()=tmp;
            speed_bg_moving.first=-speed_bg_moving.first;
        }
        tmp=-(size_pixmap_crt.rheight()-view_title->height())/2;
        if(pos.ry()<tmp)
        {
            pos.ry()=tmp;
            speed_bg_moving.second=-speed_bg_moving.second;
        }
        tmp=-tmp;
        if(pos.ry()>tmp)
        {
            pos.ry()=tmp;
            speed_bg_moving.second=-speed_bg_moving.second;
        }

        bg.setPos(pos);//更新位置
    }


    if(count%Settings::period_change_speed==0)//更新移动速度
    {
        //水平速度
        BinaryVector<Decimal> speed_polar{0,Settings::speed_bg_moving};
        speed_polar.first=2*PI*ToolFunctionsKit::get_random_decimal_0_1();//方
        ToolFunctionsKit::polar_to_axis(speed_polar,speed_bg_moving);//极转轴
    }

    if(count%Settings::period_change_title_bg==0)//更换图片
    {
        ++index_crt;
        index_crt%=size;
        this->bg.setPixmap(images_title[index_crt]);
        size_pixmap_crt=images_title[index_crt].size();
        bg.setOffset(-size_pixmap_crt.rwidth()/2, -size_pixmap_crt.rheight()/2);//设置中心偏移

        bg.setPos(0,0);//置于中心
    }


    ++count;
}

void GameWidget::push_info(const QString &message)
{
    this->browser_info->append(message);
    this->browser_info->update();//刷新
}

void GameWidget::goto_start_page()
{
    this->goto_page(StartPage);
    this->load_scene_list();//加载场景列表
}

void GameWidget::goto_title_page()
{
    this->goto_page(TitlePage);
}

void GameWidget::goto_log_page()
{
    this->button_to_info_page_pause_menu->setEnabled(false);//不可用
    this->goto_page(LogPage);
    esc();
}

void GameWidget::close_log_page()
{
    this->button_to_info_page_pause_menu->setEnabled(true);//不可用
    goto_page(page_last);//返回
}

void GameWidget::exit()
{
    Page page_crt=static_cast<Page>(widget_main->currentIndex());

    switch(page_crt)
    {
    case TitlePage:
    {

    }
    [[clang::fallthrough]];
    case StartPage:
    {
        break;
    }
    case LogPage:
    {
        this->close_log_page();
        break;
    }
    case GamePage:
    {
        reset();
        break;
    }
    case OptionPage:
    {
        break;
    }

    }
    esc();

}

void GameWidget::game_over()
{

}


void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat()) //排除自动重复的键盘事件
    {
        unsigned code_key = event->nativeVirtualKey();

        if (Settings::map_keys.find(code_key) != Settings::map_keys.end())
        {
            //设置对应键位按下状态
            data_runtime.status_keys[Settings::map_keys[code_key]] = true;
        }
        key_process();
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat()) //排除自动重复的键盘事件
    {
        unsigned code_key = event->nativeVirtualKey();

        if (Settings::map_keys.find(code_key) != Settings::map_keys.end())
        {
            //设置对应键位释放状态
            data_runtime.status_keys[Settings::map_keys[code_key]] = false;
        }
    }
}

void GameWidget::mousePressEvent(QMouseEvent *event)
{
    if(this->status!=Status::Running)//检查运行状态
        return;
    if (event->buttons() & Qt::LeftButton)
    {
        data_runtime.status_keys[Key::ML] = true;
//        mouse_delay = 7;
        mouse_event_not_handled=true;
    }
    if (event->buttons() & Qt::RightButton)
    {
        data_runtime.status_keys[Key::MR] = true;
    }
}

void GameWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if(this->status!=Status::Running)//检查运行状态
        return;
    if (!event->buttons() & Qt::LeftButton)
    {
        if(!mouse_event_not_handled)
            data_runtime.status_keys[Key::ML] = false;
    }
    if (!event->buttons() & Qt::RightButton)
    {
        data_runtime.status_keys[Key::MR] = false;
    }
}

void GameWidget::resizeEvent(QResizeEvent *event)
{
    this->view_title->resize(event->size());
    this->widget_menu->resize(event->size());
    this->widget_game_info->resize(event->size());

//    data_runtime.view_main->setSceneRect(0,0,data_runtime.view_main->width()-1,data_runtime.view_main->height()-1);
    view_title->setSceneRect(-view_title->width()/2,-view_title->height()/2,view_title->width(),view_title->height());
}

//void GameWidget::moveEvent(QMoveEvent *event)
//{

//}









