# 遥控驱动调试记录（中文）

本文是现场调试记录，不是最终项目 README。

## 1. 调试目的与当前架构

```text
FS-i6X 遥控器
  -> FS-iA10B 接收机
  -> iBUS
  -> A 板 STM32 USART2 RX
  -> CH1/CH2/CH7/CH9 映射
  -> 左右轮目标转速
  -> 速度 PID
  -> CAN
  -> C620
  -> M3508
  -> 左右螺旋推进轮
```

当前阶段只控制原有左右两个 C620/M3508，第三、第四电机目标和电流保持为 0。

## 2. 遥控器通道映射表

| 通道 | 控件 | 当前用途 |
| --- | --- | --- |
| CH1 | 右摇杆左右 | `turn` |
| CH2 | 右摇杆上下 | `forward` |
| CH3 | 左摇杆上下 | `aux1`，预留 |
| CH4 | 左摇杆左右 | `aux2`，预留 |
| CH5 | 左侧旋钮 | 预留 |
| CH6 | 右侧旋钮 | 预留 |
| CH7 | SA | ARM / DISARM |
| CH8 | SB | 预留 |
| CH9 | SC | LOW / MID / HIGH |
| CH10 | SD | 预留 |

```text
SA 上方约 1000：DISARM
SA 下方约 2000：ARM 请求

SC 上方约 1000：LOW
SC 中间约 1500：MID
SC 下方约 2000：HIGH
```

摇杆通道归一化规则：

```text
1000 -> -1.0
1500 ->  0.0
2000 -> +1.0
```

`REMOTE_STICK_DEADZONE = 0.05f`，死区内置零，死区外保持线性比例；满杆仍为 `-1.0` / `+1.0`。

## 3. 当前对比测试标定与速度单位

```text
原始 zip 固定速度：
左轮 -360，右轮 +360。

当前遥控 HIGH 满杆：
左轮 -400，右轮 +400。

当前目标：
遥控器满杆效果应略高于原始固定 ±360 测试，
而不是按电机铭牌最高转速运行。

LOW  = 160
MID  = 280
HIGH = 400
```

满杆时达到当前档位最大目标转速。实际转速会受电源、电机负载、螺旋推进轮阻力、PID 参数和电流限幅影响。

当前速度数值是原始 C620 反馈/PID 链路中的控制单位，用于复现原始 Demo 的体感和运动效果。它不应被直接解释为减速后输出轴的最终机械 rpm。

`set_spd[]`、`moto_chassis[].speed_rpm` 和 PID 目标值统一使用这套原始控制单位。原始 Demo 中的 `-360 / +360` 只是直接写入 `set_spd[]` 的速度设定值，没有任何减速比换算。

## 4. 当前代码方向表

当前方向宏：

```c
#define REMOTE_FORWARD_DIR  (+1.0f)
#define REMOTE_TURN_DIR     (+1.0f)
#define LEFT_WHEEL_DIR      (-1.0f)
#define RIGHT_WHEEL_DIR     (+1.0f)
```

当前代码混控公式：

```c
forward_cmd = REMOTE_FORWARD_DIR * forward;
turn_cmd    = REMOTE_TURN_DIR    * turn;

left_mix_raw  = forward_cmd + turn_cmd;
right_mix_raw = forward_cmd - turn_cmd;

mix_scale = max(1.0f, abs(left_mix_raw), abs(right_mix_raw));

left_mix  = left_mix_raw  / mix_scale;
right_mix = right_mix_raw / mix_scale;

left_target_pid_speed  = LEFT_WHEEL_DIR  * left_mix  * selected_pid_speed_limit;
right_target_pid_speed = RIGHT_WHEEL_DIR * right_mix * selected_pid_speed_limit;
```

| 操作 | forward | turn | HIGH 档左轮目标 | HIGH 档右轮目标 | 代码暂定运动含义 | 实测结果 | 是否确认 |
| --- | ---: | ---: | ---: | ---: | --- | --- | --- |
| 右摇杆上推 | +1 | 0 | -400 | +400 | 暂定前进，待实测确认 | 待填写 | 否 |
| 右摇杆下拉 | -1 | 0 | +400 | -400 | 暂定后退，待实测确认 | 待填写 | 否 |
| 右摇杆右推 | 0 | +1 | -400 | -400 | 暂定右转，待实测确认 | 待填写 | 否 |
| 右摇杆左推 | 0 | -1 | +400 | +400 | 暂定左转，待实测确认 | 待填写 | 否 |

后续修正原则：

1. 若右摇杆上推时整机实际后退，优先修改 `REMOTE_FORWARD_DIR` 的正负号。
2. 若右摇杆右推时整机实际向相反方向转向，优先修改 `REMOTE_TURN_DIR` 的正负号。
3. 若只有一侧螺旋推进轮方向与另一侧不匹配，先检查该侧推进轮安装朝向、螺旋手性和电机方向，确认后再修改 `LEFT_WHEEL_DIR` 或 `RIGHT_WHEEL_DIR`。
4. 不通过交换 CAN ID、交换电机槽位或调换 C620 线束来修正软件方向。

## 5. 螺旋推进轮方向确认流程

1. 明确定义机器人前方。
2. 明确定义观察每个推进轮旋转方向的视角，例如从机器人外侧看向轮心。
3. 记录左、右推进轮的螺旋手性和安装方向。
4. 在 LOW 档测试右摇杆上、下、左、右。
5. 记录两侧轮子的实际旋向。
6. 记录整机实际前进、后退、左转、右转或偏航趋势。
7. 根据结果决定修改 `REMOTE_FORWARD_DIR`、`REMOTE_TURN_DIR`、`LEFT_WHEEL_DIR` 或 `RIGHT_WHEEL_DIR`。
8. 未确认前禁止把“前进/后退/左转/右转”写成最终结论。

## 6. LED 状态表

| LED 表现 | 当前代码状态 | 含义 | 现场操作 |
| --- | --- | --- | --- |
| 常灭 | `REMOTE_DRIVE_ENABLE == 0` 或 `IBUS_DIAGNOSTIC_ONLY != 0` | 驱动软件禁用 | 检查 `REMOTE_DRIVE_ENABLE` / `IBUS_DIAGNOSTIC_ONLY` |
| 快闪 | iBUS 串口离线 / 遥控链路异常 | 接收机或 iBUS 链路异常 | 查接收机、电源、PD6、共地和 FailSafe |
| 双闪后停顿 | 未完成上电后 SA 安全位流程 | 尚未看到 SA 上方安全位 | SA 上拨并保持 |
| 三连短闪 | CH1/CH2 未回中，ARM 被拒绝 | 摇杆未回中 | 右摇杆回中后 SA 上拨再下拨 |
| 慢闪 | 正常 DISARM | 未放行驱动 | 检查 SA 状态 |
| 常亮 | `remote_cmd.arm_state == REMOTE_ARMED` 且 `drive_gate_open == 1` | 可驱动左右轮 | 可进行 LOW 档验证 |

C620 反馈在线状态只作为 `motorL_diag` / `motorR_diag` 观察字段，不再作为 LED 常亮或驱动门条件。

## 7. 调试记录模板

```text
日期：
固件配置：
  IBUS_DIAGNOSTIC_ONLY =
  REMOTE_DRIVE_ENABLE =
  REMOTE_PID_SPEED_LOW =
  REMOTE_PID_SPEED_MID =
  REMOTE_PID_SPEED_HIGH =
  REMOTE_TARGET_RAMP_ENABLE =
  REMOTE_C620_CURRENT_RAW_LIMIT =

方向宏：
  REMOTE_FORWARD_DIR =
  REMOTE_TURN_DIR =
  LEFT_WHEEL_DIR =
  RIGHT_WHEEL_DIR =

测试环境：
  电源电压：
  是否悬空：
  是否安装螺旋推进轮：
  螺旋推进轮手性/安装方向：

测试动作：
  SC 档位：
  SA 状态：
  右摇杆动作：
  串口 DRIVE 行：
  左轮旋向：
  右轮旋向：
  整机运动趋势：
  异常现象：
  下一步处理：
```
