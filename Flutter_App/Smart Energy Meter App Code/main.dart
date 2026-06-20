import 'dart:async';
import 'dart:math' as math;

import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

import 'firebase_options.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
  runApp(const SmartEnergyMeterApp());
}

class SmartEnergyMeterApp extends StatelessWidget {
  const SmartEnergyMeterApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Smart Energy Meter',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF07111F),
        useMaterial3: true,
      ),
      home: const MainShell(),
    );
  }
}

class MainShell extends StatefulWidget {
  const MainShell({super.key});

  @override
  State<MainShell> createState() => _MainShellState();
}

class _MainShellState extends State<MainShell> {
  int selectedIndex = 0;

  final DatabaseReference deviceRef =
      FirebaseDatabase.instance.ref('devices/device001');

  @override
  Widget build(BuildContext context) {
    final pages = [
      HomePage(deviceRef: deviceRef),
      CostPage(deviceRef: deviceRef),
      HistoryPage(deviceRef: deviceRef),
      AnalysisPage(deviceRef: deviceRef),
      SettingsPage(deviceRef: deviceRef),
    ];

    return Scaffold(
      body: pages[selectedIndex],
      bottomNavigationBar: NavigationBar(
        selectedIndex: selectedIndex,
        backgroundColor: const Color(0xFF0A1628),
        indicatorColor: Colors.cyanAccent.withOpacity(0.18),
        onDestinationSelected: (i) => setState(() => selectedIndex = i),
        labelBehavior: NavigationDestinationLabelBehavior.alwaysShow,
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.home_outlined),
            selectedIcon: Icon(Icons.home),
            label: 'Home',
          ),
          NavigationDestination(
            icon: Icon(Icons.payments_outlined),
            selectedIcon: Icon(Icons.payments),
            label: 'Cost',
          ),
          NavigationDestination(
            icon: Icon(Icons.history_outlined),
            selectedIcon: Icon(Icons.history),
            label: 'History',
          ),
          NavigationDestination(
            icon: Icon(Icons.analytics_outlined),
            selectedIcon: Icon(Icons.analytics),
            label: 'Analysis',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}

Map<String, dynamic> snapMap(DataSnapshot snapshot) {
  final value = snapshot.value;
  if (value is! Map) return {};
  return Map<String, dynamic>.from(
    value.map((key, val) => MapEntry(key.toString(), val)),
  );
}

Map<String, dynamic> dynamicMap(dynamic value) {
  if (value is! Map) return {};
  return Map<String, dynamic>.from(
    value.map((key, val) => MapEntry(key.toString(), val)),
  );
}

double asDouble(dynamic value) {
  if (value is int) return value.toDouble();
  if (value is double) return value;
  if (value is num) return value.toDouble();
  return 0.0;
}

int asInt(dynamic value) {
  if (value is int) return value;
  if (value is num) return value.toInt();
  return 0;
}

bool asBool(dynamic value) {
  if (value is bool) return value;
  if (value is int) return value == 1;
  if (value is String) return value.toLowerCase() == 'true';
  return false;
}

String asString(dynamic value, [String fallback = '']) {
  if (value is String) return value;
  if (value == null) return fallback;
  return value.toString();
}

bool isDeviceOnline(Map<String, dynamic> live) {
  final lastSeenEpoch = asInt(live['lastSeenEpoch']);
  if (lastSeenEpoch <= 0) return false;

  final nowEpoch = DateTime.now().millisecondsSinceEpoch ~/ 1000;
  final ageSeconds = nowEpoch - lastSeenEpoch;

  return ageSeconds >= 0 && ageSeconds <= 15;
}

class Header extends StatelessWidget {
  final String title;
  final String subtitle;
  final Color statusColor;

  const Header({
    super.key,
    required this.title,
    required this.subtitle,
    required this.statusColor,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                title,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(
                  fontSize: 23,
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 5),
              Row(
                children: [
                  Container(
                    width: 9,
                    height: 9,
                    decoration: BoxDecoration(
                      color: statusColor,
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      subtitle,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: TextStyle(color: Colors.white.withOpacity(0.70)),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: Colors.cyanAccent.withOpacity(0.12),
            borderRadius: BorderRadius.circular(16),
          ),
          child: const Icon(Icons.notifications_none, color: Colors.cyanAccent),
        ),
      ],
    );
  }
}

class GlassCard extends StatelessWidget {
  final Widget child;
  final EdgeInsetsGeometry padding;

  const GlassCard({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(14),
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: padding,
      decoration: BoxDecoration(
        color: const Color(0xFF0D1D33),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: Colors.white.withOpacity(0.08)),
        boxShadow: [
          BoxShadow(
            color: Colors.cyanAccent.withOpacity(0.06),
            blurRadius: 22,
            offset: const Offset(0, 10),
          ),
        ],
      ),
      child: child,
    );
  }
}

class BigText extends StatelessWidget {
  final String title;
  final String value;

  const BigText({
    super.key,
    required this.title,
    required this.value,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          title,
          style: TextStyle(color: Colors.white.withOpacity(0.65)),
        ),
        const SizedBox(height: 8),
        FittedBox(
          alignment: Alignment.centerLeft,
          fit: BoxFit.scaleDown,
          child: Text(
            value,
            style: const TextStyle(
              fontSize: 26,
              fontWeight: FontWeight.bold,
              color: Colors.cyanAccent,
            ),
          ),
        ),
      ],
    );
  }
}

class EnergyCircle extends StatelessWidget {
  final double value;
  final double maxValue;

  const EnergyCircle({
    super.key,
    required this.value,
    required this.maxValue,
  });

  @override
  Widget build(BuildContext context) {
    final screenWidth = MediaQuery.of(context).size.width;
    final size = math.min(screenWidth * 0.68, 250.0);

    return SizedBox(
      width: size,
      height: size,
      child: Stack(
        alignment: Alignment.center,
        children: [
          CustomPaint(
            size: Size(size, size),
            painter: RingPainter((value / maxValue).clamp(0.0, 1.0)),
          ),
          Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(
                'SESSION ENERGY',
                style: TextStyle(
                  color: Colors.cyanAccent.withOpacity(0.8),
                  letterSpacing: 2,
                  fontSize: 13,
                ),
              ),
              const SizedBox(height: 8),
              FittedBox(
                child: Text(
                  value.toStringAsFixed(5),
                  style: const TextStyle(
                    fontSize: 36,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
              const Text(
                'kWh',
                style: TextStyle(
                  fontSize: 18,
                  color: Colors.cyanAccent,
                ),
              ),
              const SizedBox(height: 8),
              Text(
                '0 - 100000 kWh',
                style: TextStyle(
                  color: Colors.white.withOpacity(0.45),
                  fontSize: 12,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class RingPainter extends CustomPainter {
  final double progress;

  RingPainter(this.progress);

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final rect = Rect.fromCircle(
      center: center,
      radius: size.width / 2 - 18,
    );

    final bg = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 16
      ..strokeCap = StrokeCap.round
      ..color = Colors.white.withOpacity(0.08);

    final fg = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 16
      ..strokeCap = StrokeCap.round
      ..shader = const LinearGradient(
        colors: [
          Colors.cyanAccent,
          Colors.blueAccent,
          Colors.purpleAccent,
        ],
      ).createShader(rect);

    canvas.drawArc(rect, -math.pi * 1.25, math.pi * 1.5, false, bg);
    canvas.drawArc(
      rect,
      -math.pi * 1.25,
      math.pi * 1.5 * progress,
      false,
      fg,
    );
  }

  @override
  bool shouldRepaint(covariant RingPainter oldDelegate) {
    return oldDelegate.progress != progress;
  }
}

class MetricCard extends StatelessWidget {
  final String title;
  final double value;
  final String unit;
  final double max;
  final IconData icon;
  final Color color;
  final bool prefixUnit;

  const MetricCard({
    super.key,
    required this.title,
    required this.value,
    required this.unit,
    required this.max,
    required this.icon,
    required this.color,
    this.prefixUnit = false,
  });

  @override
  Widget build(BuildContext context) {
    final progress = (value / max).clamp(0.0, 1.0);
    final decimals = title == 'Current' || title == 'Max Current' ? 2 : 1;

    final text = prefixUnit
        ? '$unit ${value.toStringAsFixed(2)}'
        : '${value.toStringAsFixed(decimals)} $unit';

    return GlassCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(icon, color: color, size: 27),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  title,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    color: Colors.white.withOpacity(0.75),
                    fontSize: 15,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          FittedBox(
            fit: BoxFit.scaleDown,
            alignment: Alignment.centerLeft,
            child: Text(
              text,
              maxLines: 1,
              style: const TextStyle(
                fontSize: 26,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
          const SizedBox(height: 12),
          ClipRRect(
            borderRadius: BorderRadius.circular(10),
            child: LinearProgressIndicator(
              value: progress,
              minHeight: 7,
              color: color,
              backgroundColor: Colors.white.withOpacity(0.08),
            ),
          ),
          const SizedBox(height: 6),
          Text(
            '0 - ${max.toInt()}',
            style: TextStyle(
              fontSize: 11,
              color: Colors.white.withOpacity(0.45),
            ),
          ),
        ],
      ),
    );
  }
}

class HomePage extends StatelessWidget {
  final DatabaseReference deviceRef;

  const HomePage({
    super.key,
    required this.deviceRef,
  });

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<DatabaseEvent>(
      stream: deviceRef.child('live').onValue,
      builder: (context, snapshot) {
        final live = snapshot.hasData
            ? snapMap(snapshot.data!.snapshot)
            : <String, dynamic>{};

        final online = isDeviceOnline(live);

        final voltage = online ? asDouble(live['voltage']) : 0.0;
        final current = online ? asDouble(live['current']) : 0.0;
        final power = online ? asDouble(live['power']) : 0.0;
        final energy = online ? asDouble(live['sessionEnergy']) : 0.0;
        final cost = online ? asDouble(live['sessionCost']) : 0.0;

        final relay = online && asInt(live['relay']) == 1;
        final mains = online && asBool(live['mainsAvailable']);
        final status =
            online ? asString(live['plugStatus'], 'UNKNOWN') : 'DEVICE_OFFLINE';

        final timerActive = online && asBool(live['timerActive']);
        final timerRemainingSec =
            online ? asInt(live['timerRemainingSec']) : 0;
        final timerSetMinutes = online ? asInt(live['timerSetMinutes']) : 0;

        return SafeArea(
          child: ListView(
            padding: const EdgeInsets.fromLTRB(18, 12, 18, 18),
            children: [
              Header(
                title: 'Smart Energy Meter',
                subtitle: online ? 'Device Online' : 'Device Offline',
                statusColor: online ? Colors.greenAccent : Colors.redAccent,
              ),
              const SizedBox(height: 14),
              Center(
                child: EnergyCircle(
                  value: energy,
                  maxValue: 100000,
                ),
              ),
              const SizedBox(height: 14),
              GridView.count(
                crossAxisCount: 2,
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                childAspectRatio: 1.03,
                crossAxisSpacing: 12,
                mainAxisSpacing: 12,
                children: [
                  MetricCard(
                    title: 'Voltage',
                    value: voltage,
                    unit: 'V',
                    max: 500,
                    icon: Icons.electric_bolt,
                    color: Colors.cyanAccent,
                  ),
                  MetricCard(
                    title: 'Current',
                    value: current,
                    unit: 'A',
                    max: 30,
                    icon: Icons.waves,
                    color: Colors.orangeAccent,
                  ),
                  MetricCard(
                    title: 'Power',
                    value: power,
                    unit: 'W',
                    max: 60000,
                    icon: Icons.power,
                    color: Colors.blueAccent,
                  ),
                  MetricCard(
                    title: 'Cost',
                    value: cost,
                    unit: 'Rs',
                    max: 10000,
                    icon: Icons.payments,
                    color: Colors.greenAccent,
                    prefixUnit: true,
                  ),
                ],
              ),
              GlassCard(
                child: Row(
                  children: [
                    Icon(
                      mains ? Icons.power_settings_new : Icons.power_off,
                      color: mains ? Colors.greenAccent : Colors.redAccent,
                      size: 34,
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            mains ? 'Mains Available' : 'Plug is OFF',
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          const SizedBox(height: 4),
                          Text(
                            'Status: $status',
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: TextStyle(
                              color: Colors.white.withOpacity(0.65),
                            ),
                          ),
                        ],
                      ),
                    ),
                    Switch(
                      value: relay,
                      activeColor: Colors.cyanAccent,
                      onChanged: online
                          ? (value) => deviceRef
                              .child('control/relayCommand')
                              .set(value ? 1 : 0)
                          : null,
                    ),
                  ],
                ),
              ),
              TimerControlCard(
                deviceRef: deviceRef,
                online: online,
                timerActive: timerActive,
                timerRemainingSec: timerRemainingSec,
                timerSetMinutes: timerSetMinutes,
              ),
            ],
          ),
        );
      },
    );
  }
}

class TimerControlCard extends StatefulWidget {
  final DatabaseReference deviceRef;
  final bool online;
  final bool timerActive;
  final int timerRemainingSec;
  final int timerSetMinutes;

  const TimerControlCard({
    super.key,
    required this.deviceRef,
    required this.online,
    required this.timerActive,
    required this.timerRemainingSec,
    required this.timerSetMinutes,
  });

  @override
  State<TimerControlCard> createState() => _TimerControlCardState();
}

class _TimerControlCardState extends State<TimerControlCard> {
  final TextEditingController timerController =
      TextEditingController(text: '10');

  Timer? localTimer;
  int localRemainingSec = 0;

  @override
  void initState() {
    super.initState();
    localRemainingSec = widget.timerRemainingSec;

    localTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (!mounted) return;
      if (widget.timerActive && localRemainingSec > 0) {
        setState(() {
          localRemainingSec--;
        });
      }
    });
  }

  @override
  void didUpdateWidget(covariant TimerControlCard oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (widget.timerRemainingSec != oldWidget.timerRemainingSec) {
      localRemainingSec = widget.timerRemainingSec;
    }

    if (!widget.timerActive) {
      localRemainingSec = 0;
    }
  }

  @override
  void dispose() {
    localTimer?.cancel();
    timerController.dispose();
    super.dispose();
  }

  String formatRemaining(int seconds) {
    if (seconds <= 0) return '00:00';
    final minutes = seconds ~/ 60;
    final sec = seconds % 60;
    return '${minutes.toString().padLeft(2, '0')}:${sec.toString().padLeft(2, '0')}';
  }

  Future<void> startTimer() async {
    final minutes = int.tryParse(timerController.text.trim()) ?? 0;

    if (minutes <= 0) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Enter valid timer minutes')),
      );
      return;
    }

    setState(() {
      localRemainingSec = minutes * 60;
    });

    await widget.deviceRef.child('control/timerMinutes').set(minutes);
    await widget.deviceRef.child('control/timerStart').set(true);
    await widget.deviceRef.child('control/relayCommand').set(1);

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Timer started for $minutes min')),
      );
    }
  }

  Future<void> cancelTimer() async {
    setState(() {
      localRemainingSec = 0;
    });

    await widget.deviceRef.child('control/timerCancel').set(true);

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Timer cancel request sent')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final remaining = formatRemaining(localRemainingSec);

    return GlassCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                widget.timerActive ? Icons.timer : Icons.timer_outlined,
                color:
                    widget.timerActive ? Colors.orangeAccent : Colors.cyanAccent,
                size: 28,
              ),
              const SizedBox(width: 10),
              Expanded(
                child: Text(
                  widget.timerActive ? 'Timer Running' : 'Auto OFF Timer',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
              if (widget.timerActive)
                Text(
                  remaining,
                  style: const TextStyle(
                    color: Colors.orangeAccent,
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            widget.timerActive
                ? 'Plug will turn OFF automatically. Set: ${widget.timerSetMinutes} min'
                : 'Example: enter 10 to turn ON plug for 10 minutes.',
            style: TextStyle(color: Colors.white.withOpacity(0.65)),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: TextField(
                  controller: timerController,
                  enabled: widget.online && !widget.timerActive,
                  keyboardType: TextInputType.number,
                  decoration: const InputDecoration(
                    labelText: 'Minutes',
                    prefixIcon: Icon(Icons.timer),
                  ),
                ),
              ),
              const SizedBox(width: 10),
              FilledButton(
                onPressed:
                    widget.online && !widget.timerActive ? startTimer : null,
                child: const Text('Start'),
              ),
              const SizedBox(width: 8),
              IconButton.filledTonal(
                onPressed:
                    widget.online && widget.timerActive ? cancelTimer : null,
                icon: const Icon(Icons.stop),
                tooltip: 'Cancel Timer',
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class CostPage extends StatelessWidget {
  final DatabaseReference deviceRef;

  const CostPage({
    super.key,
    required this.deviceRef,
  });

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: StreamBuilder<DatabaseEvent>(
        stream: deviceRef.child('live').onValue,
        builder: (context, snapshot) {
          final live = snapshot.hasData
              ? snapMap(snapshot.data!.snapshot)
              : <String, dynamic>{};

          final online = isDeviceOnline(live);
          final energy = online ? asDouble(live['sessionEnergy']) : 0.0;
          final cost = online ? asDouble(live['sessionCost']) : 0.0;
          final unitPrice =
              asDouble(live['unitPrice']) == 0 ? 26.0 : asDouble(live['unitPrice']);

          return ListView(
            padding: const EdgeInsets.all(18),
            children: [
              Header(
                title: 'Cost',
                subtitle:
                    online ? 'Sri Lanka Rs / LKR tracking' : 'Device Offline',
                statusColor: online ? Colors.greenAccent : Colors.redAccent,
              ),
              const SizedBox(height: 16),
              GlassCard(
                child: BigText(
                  title: 'Current Session Cost',
                  value: 'Rs ${cost.toStringAsFixed(2)}',
                ),
              ),
              GlassCard(
                child: BigText(
                  title: 'Energy Used',
                  value: '${energy.toStringAsFixed(5)} kWh',
                ),
              ),
              GlassCard(
                child: BigText(
                  title: 'Unit Price',
                  value: 'Rs ${unitPrice.toStringAsFixed(2)} / kWh',
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

class HistoryPage extends StatelessWidget {
  final DatabaseReference deviceRef;

  const HistoryPage({
    super.key,
    required this.deviceRef,
  });

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: StreamBuilder<DatabaseEvent>(
        stream: deviceRef.child('history').onValue,
        builder: (context, snapshot) {
          final data = snapshot.hasData
              ? snapMap(snapshot.data!.snapshot)
              : <String, dynamic>{};

          final entries = data.entries.toList()
            ..sort((a, b) => b.key.compareTo(a.key));

          return ListView(
            padding: const EdgeInsets.all(18),
            children: [
              const Header(
                title: 'History',
                subtitle: 'Previous plug sessions',
                statusColor: Colors.cyanAccent,
              ),
              const SizedBox(height: 16),
              if (entries.isEmpty)
                const GlassCard(
                  child: Text('No history yet. Turn relay OFF after a session.'),
                ),
              ...entries.map((entry) {
                final item = dynamicMap(entry.value);
                final duration = asInt(item['durationSec']);

                return GlassCard(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        asString(item['startTime'], 'Unknown'),
                        style: const TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                          color: Colors.cyanAccent,
                        ),
                      ),
                      const SizedBox(height: 6),
                      Text('OFF time: ${asString(item['endTime'], 'Unknown')}'),
                      Text('Duration: ${(duration / 60).toStringAsFixed(1)} min'),
                      Text('Reason: ${asString(item['reason'], 'unknown')}'),
                      const SizedBox(height: 8),
                      Text(
                        'Energy: ${asDouble(item['energy']).toStringAsFixed(5)} kWh',
                      ),
                      Text(
                        'Cost: Rs ${asDouble(item['cost']).toStringAsFixed(2)}',
                      ),
                      Text(
                        'Max Power: ${asDouble(item['maxPower']).toStringAsFixed(1)} W',
                      ),
                      Text(
                        'Max Current: ${asDouble(item['maxCurrent']).toStringAsFixed(2)} A',
                      ),
                    ],
                  ),
                );
              }),
            ],
          );
        },
      ),
    );
  }
}

class AnalysisPage extends StatelessWidget {
  final DatabaseReference deviceRef;

  const AnalysisPage({
    super.key,
    required this.deviceRef,
  });

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: StreamBuilder<DatabaseEvent>(
        stream: deviceRef.child('history').onValue,
        builder: (context, snapshot) {
          final data = snapshot.hasData
              ? snapMap(snapshot.data!.snapshot)
              : <String, dynamic>{};

          double totalEnergy = 0;
          double totalCost = 0;
          double maxPower = 0;
          double maxCurrent = 0;
          final sessions = <Map<String, dynamic>>[];

          for (final v in data.values) {
            final item = dynamicMap(v);
            if (item.isNotEmpty) {
              sessions.add(item);
              totalEnergy += asDouble(item['energy']);
              totalCost += asDouble(item['cost']);
              maxPower = math.max(maxPower, asDouble(item['maxPower']));
              maxCurrent = math.max(maxCurrent, asDouble(item['maxCurrent']));
            }
          }

          sessions.sort(
            (a, b) => asString(b['startTime']).compareTo(
              asString(a['startTime']),
            ),
          );

          return ListView(
            padding: const EdgeInsets.all(18),
            children: [
              const Header(
                title: 'Analysis',
                subtitle: 'Energy usage insights',
                statusColor: Colors.purpleAccent,
              ),
              const SizedBox(height: 16),
              GridView.count(
                crossAxisCount: 2,
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                childAspectRatio: 1.03,
                crossAxisSpacing: 12,
                mainAxisSpacing: 12,
                children: [
                  MetricCard(
                    title: 'Total Energy',
                    value: totalEnergy,
                    unit: 'kWh',
                    max: 100000,
                    icon: Icons.battery_charging_full,
                    color: Colors.cyanAccent,
                  ),
                  MetricCard(
                    title: 'Total Cost',
                    value: totalCost,
                    unit: 'Rs',
                    max: 100000,
                    icon: Icons.payments,
                    color: Colors.greenAccent,
                    prefixUnit: true,
                  ),
                  MetricCard(
                    title: 'Max Power',
                    value: maxPower,
                    unit: 'W',
                    max: 60000,
                    icon: Icons.bolt,
                    color: Colors.blueAccent,
                  ),
                  MetricCard(
                    title: 'Max Current',
                    value: maxCurrent,
                    unit: 'A',
                    max: 30,
                    icon: Icons.waves,
                    color: Colors.orangeAccent,
                  ),
                ],
              ),
              SessionMinuteCostChart(deviceRef: deviceRef),
            ],
          );
        },
      ),
    );
  }
}

class SessionMinuteCostChart extends StatelessWidget {
  final DatabaseReference deviceRef;

  const SessionMinuteCostChart({
    super.key,
    required this.deviceRef,
  });

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<DatabaseEvent>(
      stream: deviceRef.child('currentMinuteCost').onValue,
      builder: (context, snapshot) {
        final data = snapshot.hasData
            ? snapMap(snapshot.data!.snapshot)
            : <String, dynamic>{};

        final rawPoints = <MinuteCostPoint>[];

        for (final value in data.values) {
          final item = dynamicMap(value);
          if (item.isNotEmpty) {
            rawPoints.add(
              MinuteCostPoint(
                minute: asInt(item['minute']),
                cost: asDouble(item['cost']),
                energy: asDouble(item['energy']),
              ),
            );
          }
        }

        rawPoints.sort((a, b) => a.minute.compareTo(b.minute));
        final points = buildHourlyCostPoints(rawPoints);
        final chartWidth = math.max(720.0, points.length * 42.0);

        return GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'Hourly Cost Line Graph',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 6),
              Text(
                'Blue line = cost increase. Red dot = highest cost point in each hour. Swipe left/right to see past hours.',
                style: TextStyle(color: Colors.white.withOpacity(0.65)),
              ),
              const SizedBox(height: 14),
              if (points.isEmpty)
                const SizedBox(
                  height: 150,
                  child: Center(
                    child: Text(
                      'No graph data yet. Keep plug ON for at least 1 minute.',
                    ),
                  ),
                )
              else
                SingleChildScrollView(
                  scrollDirection: Axis.horizontal,
                  child: SizedBox(
                    height: 260,
                    width: chartWidth,
                    child: CustomPaint(
                      painter: HourlyCostLineChartPainter(points),
                    ),
                  ),
                ),
            ],
          ),
        );
      },
    );
  }
}

class MinuteCostPoint {
  final int minute;
  final double cost;
  final double energy;

  MinuteCostPoint({
    required this.minute,
    required this.cost,
    required this.energy,
  });
}

class HourlyCostPoint {
  final int minute;
  final int hourIndex;
  final double totalCost;
  final double hourlyCost;
  final bool isHourMax;

  HourlyCostPoint({
    required this.minute,
    required this.hourIndex,
    required this.totalCost,
    required this.hourlyCost,
    required this.isHourMax,
  });
}

List<HourlyCostPoint> buildHourlyCostPoints(List<MinuteCostPoint> rawPoints) {
  if (rawPoints.isEmpty) return [];

  final baseCostByHour = <int, double>{};
  final maxCostByHour = <int, double>{};

  for (final point in rawPoints) {
    final hourIndex = point.minute ~/ 60;
    baseCostByHour.putIfAbsent(hourIndex, () => point.cost);

    final hourlyCost = point.cost - (baseCostByHour[hourIndex] ?? point.cost);
    maxCostByHour[hourIndex] = math.max(
      maxCostByHour[hourIndex] ?? hourlyCost,
      hourlyCost,
    );
  }

  return rawPoints.map((point) {
    final hourIndex = point.minute ~/ 60;
    final hourlyCost = point.cost - (baseCostByHour[hourIndex] ?? point.cost);
    final hourMax = maxCostByHour[hourIndex] ?? 0.0;

    return HourlyCostPoint(
      minute: point.minute,
      hourIndex: hourIndex,
      totalCost: point.cost,
      hourlyCost: hourlyCost,
      isHourMax: hourlyCost == hourMax && hourMax > 0,
    );
  }).toList();
}

class HourlyCostLineChartPainter extends CustomPainter {
  final List<HourlyCostPoint> points;

  HourlyCostLineChartPainter(this.points);

  @override
  void paint(Canvas canvas, Size size) {
    if (points.isEmpty) return;

    final chartRect = Rect.fromLTWH(
      48,
      18,
      size.width - 72,
      size.height - 62,
    );

    final axisPaint = Paint()
      ..color = Colors.white.withOpacity(0.20)
      ..strokeWidth = 1;

    final gridPaint = Paint()
      ..color = Colors.white.withOpacity(0.08)
      ..strokeWidth = 1;

    final linePaint = Paint()
      ..color = Colors.cyanAccent
      ..strokeWidth = 2.8
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    final blueDotPaint = Paint()
      ..color = Colors.blueAccent
      ..style = PaintingStyle.fill;

    final redDotPaint = Paint()
      ..color = Colors.redAccent
      ..style = PaintingStyle.fill;

    final textPainter = TextPainter(textDirection: TextDirection.ltr);

    final maxHourlyCost = points
        .map((point) => point.hourlyCost)
        .fold<double>(0.0, math.max);
    final safeMax = maxHourlyCost <= 0 ? 1.0 : maxHourlyCost;

    canvas.drawLine(chartRect.bottomLeft, chartRect.bottomRight, axisPaint);
    canvas.drawLine(chartRect.bottomLeft, chartRect.topLeft, axisPaint);

    for (int i = 1; i <= 4; i++) {
      final y = chartRect.bottom - (chartRect.height * i / 4);
      canvas.drawLine(
        Offset(chartRect.left, y),
        Offset(chartRect.right, y),
        gridPaint,
      );

      final labelValue = safeMax * i / 4;
      textPainter.text = TextSpan(
        text: 'Rs ${labelValue.toStringAsFixed(2)}',
        style: TextStyle(
          color: Colors.white.withOpacity(0.58),
          fontSize: 10,
        ),
      );
      textPainter.layout();
      textPainter.paint(
        canvas,
        Offset(0, y - textPainter.height / 2),
      );
    }

    final path = Path();
    final pointOffsets = <Offset>[];

    for (int i = 0; i < points.length; i++) {
      final x = chartRect.left +
          (points.length == 1
              ? chartRect.width / 2
              : (i / (points.length - 1)) * chartRect.width);
      final y = chartRect.bottom -
          (points[i].hourlyCost / safeMax).clamp(0.0, 1.0) * chartRect.height;

      final offset = Offset(x, y);
      pointOffsets.add(offset);

      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }

    canvas.drawPath(path, linePaint);

    int lastHourLabel = -1;

    for (int i = 0; i < points.length; i++) {
      final point = points[i];
      final offset = pointOffsets[i];

      if (point.isHourMax) {
        canvas.drawCircle(offset, 5.5, redDotPaint);

        final label = 'Rs ${point.hourlyCost.toStringAsFixed(2)}';
        textPainter.text = TextSpan(
          text: label,
          style: const TextStyle(
            color: Colors.white,
            fontSize: 11,
            fontWeight: FontWeight.bold,
          ),
        );
        textPainter.layout();

        final labelX = (offset.dx - textPainter.width / 2)
            .clamp(chartRect.left, chartRect.right - textPainter.width);
        final labelY = (offset.dy - 22)
            .clamp(chartRect.top, chartRect.bottom - textPainter.height);

        final bgRect = Rect.fromLTWH(
          labelX - 4,
          labelY - 2,
          textPainter.width + 8,
          textPainter.height + 4,
        );

        final bgPaint = Paint()..color = Colors.black.withOpacity(0.72);
        canvas.drawRRect(
          RRect.fromRectAndRadius(bgRect, const Radius.circular(6)),
          bgPaint,
        );
        textPainter.paint(canvas, Offset(labelX, labelY));
      } else {
        canvas.drawCircle(offset, 3.2, blueDotPaint);
      }

      if (point.hourIndex != lastHourLabel) {
        lastHourLabel = point.hourIndex;

        textPainter.text = TextSpan(
          text: 'H${point.hourIndex + 1}',
          style: TextStyle(
            color: Colors.white.withOpacity(0.70),
            fontSize: 11,
            fontWeight: FontWeight.bold,
          ),
        );
        textPainter.layout();
        textPainter.paint(
          canvas,
          Offset(offset.dx - textPainter.width / 2, chartRect.bottom + 10),
        );
      }

      if (i == points.length - 1 || i % 10 == 0) {
        textPainter.text = TextSpan(
          text: '${point.minute}m',
          style: TextStyle(
            color: Colors.white.withOpacity(0.45),
            fontSize: 9,
          ),
        );
        textPainter.layout();
        textPainter.paint(
          canvas,
          Offset(offset.dx - textPainter.width / 2, chartRect.bottom + 26),
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant HourlyCostLineChartPainter oldDelegate) {
    return oldDelegate.points != points;
  }
}

class SettingsPage extends StatefulWidget {
  final DatabaseReference deviceRef;

  const SettingsPage({
    super.key,
    required this.deviceRef,
  });

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final priceController = TextEditingController(text: '26');
  final nameController = TextEditingController(text: 'Smart Plug');
  final requesterController = TextEditingController(text: 'My Phone');

  final setupSsidController = TextEditingController();
  final setupPasswordController = TextEditingController();

  bool setupSending = false;

  @override
  void dispose() {
    priceController.dispose();
    nameController.dispose();
    requesterController.dispose();
    setupSsidController.dispose();
    setupPasswordController.dispose();
    super.dispose();
  }

  Future<void> sendWiFiToESP32() async {
    final ssid = setupSsidController.text.trim();
    final password = setupPasswordController.text;

    if (ssid.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Enter Wi-Fi name')),
      );
      return;
    }

    setState(() => setupSending = true);

    try {
      final uri = Uri.http(
        '192.168.4.1',
        '/save',
        {
          'ssid': ssid,
          'password': password,
        },
      );

      final response = await http.get(uri).timeout(
            const Duration(seconds: 10),
          );

      if (!mounted) return;

      if (response.statusCode == 200) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Wi-Fi sent. ESP32 will restart and connect.'),
          ),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Setup failed: ${response.body}')),
        );
      }
    } catch (e) {
      if (!mounted) return;

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'Cannot reach ESP32. Connect phone to SmartPlug Wi-Fi first. Error: $e',
          ),
        ),
      );
    } finally {
      if (mounted) {
        setState(() => setupSending = false);
      }
    }
  }

  Future<void> saveSettings() async {
    final price = double.tryParse(priceController.text.trim()) ?? 26.0;
    await widget.deviceRef.child('settings/unitPrice').set(price);
    await widget.deviceRef
        .child('settings/deviceName')
        .set(nameController.text.trim());

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Settings saved')),
      );
    }
  }

  Future<void> confirmClear() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF0D1D33),
        title: const Text('Clear Home/Cost values?'),
        content: const Text(
          'This resets the current visible energy and cost to zero. History and Analysis data will not be deleted.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Clear'),
          ),
        ],
      ),
    );

    if (ok == true) {
      await widget.deviceRef.child('live').update({
        'sessionEnergy': 0,
        'sessionCost': 0,
      });

      await widget.deviceRef.child('control/clearSession').set(true);

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Home/Cost values cleared. History kept.'),
          ),
        );
      }
    }
  }

  Future<void> confirmDelete() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: const Color(0xFF0D1D33),
        title: const Text('Delete all history?'),
        content: const Text(
          'This will delete saved History and Analysis data. Home live values are not the main target.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: Colors.redAccent),
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );

    if (ok == true) {
      await widget.deviceRef.child('control/deleteAllHistory').set(true);

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Delete request sent')),
        );
      }
    }
  }

  Future<void> connectThisPhone(
    Map<String, dynamic> settings,
    Map<String, dynamic> pairing,
  ) async {
    final protectedMode = asBool(settings['pairingPermissionProtected']);
    final ownerExists =
        asBool(pairing['ownerExists']) || asString(pairing['ownerName']).isNotEmpty;

    final requesterName = requesterController.text.trim().isEmpty
        ? 'My Phone'
        : requesterController.text.trim();

    final requesterId = DateTime.now().millisecondsSinceEpoch.toString();

    if (!ownerExists) {
      await widget.deviceRef.child('pairing').update({
        'ownerExists': true,
        'ownerName': requesterName,
        'ownerId': requesterId,
        'updatedAt': DateTime.now().toIso8601String(),
      });

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('This phone is now the owner')),
        );
      }
      return;
    }

    if (!protectedMode) {
      await widget.deviceRef.child('pairing').update({
        'ownerExists': true,
        'ownerName': requesterName,
        'ownerId': requesterId,
        'updatedAt': DateTime.now().toIso8601String(),
      });

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Permission OFF. This phone became new owner.'),
          ),
        );
      }
      return;
    }

    await widget.deviceRef.child('pairing/pendingRequest').set({
      'active': true,
      'requestId': requesterId,
      'requesterName': requesterName,
      'requesterId': requesterId,
      'requestTime': DateTime.now().toIso8601String(),
    });

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Pairing request sent to owner')),
      );
    }
  }

  Future<void> releaseDevice() async {
    await widget.deviceRef.child('pairing').update({
      'ownerExists': false,
      'ownerName': '',
      'ownerId': '',
      'pendingRequest': {
        'active': false,
      },
      'releasedAt': DateTime.now().toIso8601String(),
    });

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Device released')),
      );
    }
  }

  Future<void> allowPendingRequest(Map<String, dynamic> pairing) async {
    final pending = dynamicMap(pairing['pendingRequest']);
    final requesterName = asString(pending['requesterName'], 'New Phone');
    final requesterId = asString(pending['requesterId']);

    if (requesterId.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Invalid request')),
      );
      return;
    }

    await widget.deviceRef.child('pairing').update({
      'ownerExists': true,
      'ownerName': requesterName,
      'ownerId': requesterId,
      'pendingRequest': {
        'active': false,
      },
      'updatedAt': DateTime.now().toIso8601String(),
    });

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Request allowed. New owner connected.')),
      );
    }
  }

  Future<void> rejectPendingRequest() async {
    await widget.deviceRef.child('pairing/pendingRequest').update({
      'active': false,
      'rejectedAt': DateTime.now().toIso8601String(),
    });

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Request rejected')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: StreamBuilder<DatabaseEvent>(
        stream: widget.deviceRef.onValue,
        builder: (context, snapshot) {
          final root = snapshot.hasData
              ? snapMap(snapshot.data!.snapshot)
              : <String, dynamic>{};

          final settings = dynamicMap(root['settings']);
          final pairing = dynamicMap(root['pairing']);
          final pending = dynamicMap(pairing['pendingRequest']);

          final protectedMode = asBool(settings['pairingPermissionProtected']);
          final ownerName = asString(pairing['ownerName'], '');
          final ownerExists = asBool(pairing['ownerExists']) || ownerName.isNotEmpty;
          final pendingActive = asBool(pending['active']);
          final requesterName = asString(pending['requesterName'], 'Unknown phone');

          return ListView(
            padding: const EdgeInsets.all(18),
            children: [
              const Header(
                title: 'Settings',
                subtitle: 'Device and cost setup',
                statusColor: Colors.cyanAccent,
              ),
              const SizedBox(height: 16),
              GlassCard(
                child: Column(
                  children: [
                    TextField(
                      controller: nameController,
                      decoration: const InputDecoration(
                        labelText: 'Device Name',
                        prefixIcon: Icon(Icons.devices),
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      controller: priceController,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(
                        labelText: 'Unit Price Rs/kWh',
                        prefixIcon: Icon(Icons.payments),
                      ),
                    ),
                    const SizedBox(height: 16),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton(
                        onPressed: saveSettings,
                        child: const Text('Save Settings'),
                      ),
                    ),
                  ],
                ),
              ),
              GlassCard(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'ESP32 Wi-Fi Setup',
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'Step 1: Press/hold Button 2 on the device.\n'
                      'Step 2: Connect phone Wi-Fi to SmartPlug_xxxx.\n'
                      'Step 3: Enter home Wi-Fi here and press Send.',
                      style: TextStyle(color: Colors.white.withOpacity(0.65)),
                    ),
                    const SizedBox(height: 14),
                    TextField(
                      controller: setupSsidController,
                      decoration: const InputDecoration(
                        labelText: 'Home Wi-Fi Name',
                        prefixIcon: Icon(Icons.wifi),
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      controller: setupPasswordController,
                      obscureText: true,
                      decoration: const InputDecoration(
                        labelText: 'Home Wi-Fi Password',
                        prefixIcon: Icon(Icons.lock),
                      ),
                    ),
                    const SizedBox(height: 16),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton.icon(
                        onPressed: setupSending ? null : sendWiFiToESP32,
                        icon: setupSending
                            ? const SizedBox(
                                width: 18,
                                height: 18,
                                child: CircularProgressIndicator(strokeWidth: 2),
                              )
                            : const Icon(Icons.send),
                        label: Text(
                          setupSending
                              ? 'Sending...'
                              : 'Send Wi-Fi to ESP32',
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              GlassCard(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Owner / Pairing Permission',
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      ownerExists
                          ? 'Current owner: $ownerName'
                          : 'No owner connected',
                      style: TextStyle(color: Colors.white.withOpacity(0.75)),
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      controller: requesterController,
                      decoration: const InputDecoration(
                        labelText: 'This phone name',
                        prefixIcon: Icon(Icons.phone_android),
                      ),
                    ),
                    const SizedBox(height: 10),
                    SwitchListTile(
                      contentPadding: EdgeInsets.zero,
                      title: const Text('Permission Protected Mode'),
                      subtitle: Text(
                        protectedMode
                            ? 'ON: new phone must request owner approval'
                            : 'OFF: new phone can connect without approval',
                      ),
                      value: protectedMode,
                      activeColor: Colors.cyanAccent,
                      onChanged: (value) => widget.deviceRef
                          .child('settings/pairingPermissionProtected')
                          .set(value),
                    ),
                    const SizedBox(height: 10),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton.icon(
                        onPressed: () => connectThisPhone(settings, pairing),
                        icon: const Icon(Icons.phone_android),
                        label: const Text('Connect This Phone / Request Access'),
                      ),
                    ),
                    const SizedBox(height: 10),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton.icon(
                        style: FilledButton.styleFrom(
                          backgroundColor: Colors.orangeAccent,
                        ),
                        onPressed: releaseDevice,
                        icon: const Icon(Icons.link_off),
                        label: const Text('Disconnect / Release Device'),
                      ),
                    ),
                    if (pendingActive) ...[
                      const SizedBox(height: 16),
                      Container(
                        padding: const EdgeInsets.all(12),
                        decoration: BoxDecoration(
                          color: Colors.orangeAccent.withOpacity(0.12),
                          borderRadius: BorderRadius.circular(16),
                          border: Border.all(
                            color: Colors.orangeAccent.withOpacity(0.35),
                          ),
                        ),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            const Text(
                              'New pairing request',
                              style: TextStyle(
                                fontSize: 16,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            const SizedBox(height: 6),
                            Text('Requester: $requesterName'),
                            const SizedBox(height: 10),
                            Row(
                              children: [
                                Expanded(
                                  child: FilledButton(
                                    onPressed: () => allowPendingRequest(pairing),
                                    child: const Text('Allow'),
                                  ),
                                ),
                                const SizedBox(width: 10),
                                Expanded(
                                  child: FilledButton(
                                    style: FilledButton.styleFrom(
                                      backgroundColor: Colors.redAccent,
                                    ),
                                    onPressed: rejectPendingRequest,
                                    child: const Text('Reject'),
                                  ),
                                ),
                              ],
                            ),
                          ],
                        ),
                      ),
                    ],
                  ],
                ),
              ),
              GlassCard(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Data Control',
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'Clear resets only Home/Cost visible energy and cost. Delete removes saved History and Analysis data.',
                      style: TextStyle(color: Colors.white.withOpacity(0.65)),
                    ),
                    const SizedBox(height: 16),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton.icon(
                        onPressed: confirmClear,
                        icon: const Icon(Icons.cleaning_services),
                        label: const Text('Clear Home/Cost Values'),
                      ),
                    ),
                    const SizedBox(height: 10),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton.icon(
                        style: FilledButton.styleFrom(
                          backgroundColor: Colors.redAccent,
                        ),
                        onPressed: confirmDelete,
                        icon: const Icon(Icons.delete_forever),
                        label: const Text('Delete History'),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}