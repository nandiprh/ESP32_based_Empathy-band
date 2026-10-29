#!/usr/bin/env python3
"""
Empathy Band - Python Streamlit Dashboard
Live monitoring dashboard for the Empathy Band IoT project
"""
import streamlit as st
import pandas as pd
import numpy as np
import time
import serial
import json
from datetime import datetime
from collections import deque
import plotly.graph_objects as go
from plotly.subplots import make_subplots

# Page configuration
st.set_page_config(
    page_title="Empathy Band - Caregiver Dashboard",
    page_icon="🩹",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Custom CSS
st.markdown("""
<style>
    .main-header {
        font-size: 2.5rem;
        font-weight: bold;
        text-align: center;
        color: #1f77b4;
        padding: 1rem;
    }
    .state-calm {
        background-color: #2f9cf4;
        color: white;
        padding: 1rem;
        border-radius: 10px;
        text-align: center;
        font-size: 2rem;
        font-weight: bold;
    }
    .state-anxious {
        background-color: #f5a623;
        color: white;
        padding: 1rem;
        border-radius: 10px;
        text-align: center;
        font-size: 2rem;
        font-weight: bold;
    }
    .state-distress {
        background-color: #ef4444;
        color: white;
        padding: 1rem;
        border-radius: 10px;
        text-align: center;
        font-size: 2rem;
        font-weight: bold;
        animation: pulse 1s infinite;
    }
    @keyframes pulse {
        0% { opacity: 1; }
        50% { opacity: 0.5; }
        100% { opacity: 1; }
    }
    .metric-card {
        background-color: #f0f2f6;
        padding: 1rem;
        border-radius: 10px;
        text-align: center;
    }
</style>
""", unsafe_allow_html=True)

# Configuration matching firmware
CFG = {
    "GSR_LEVEL_THRESH": 0.55,
    "GSR_SLOPE_THRESH": 0.08,
    "RMSSD_LOW_THRESH_MS": 30.0,
    "SOUND_THRESH": 0.50,
    "SOUND_SUSTAIN_MS": 1000,
    "MOTION_STILL_THRESH": 0.12,
    "DISTRESS_MIN_POINTS": 3,
    "ALERT_DELAY_MS": 0
}

class EmpathyBandDashboard:
    def __init__(self):
        # Data storage
        self.max_points = 300
        self.data = {
            'time': deque(maxlen=self.max_points),
            'state': deque(maxlen=self.max_points),
            'gsr': deque(maxlen=self.max_points),
            'gsr_slope': deque(maxlen=self.max_points),
            'sound': deque(maxlen=self.max_points),
            'motion': deque(maxlen=self.max_points),
            'bpm': deque(maxlen=self.max_points),
            'rmssd': deque(maxlen=self.max_points)
        }
        self.events = []
        self.session_start = time.time()
        self.state_counts = {'CALM': 0, 'ANXIOUS': 0, 'DISTRESS': 0}
        self.last_state = 'CALM'
        self.distress_start = None
        self.alerts = 0
        self.marks = 0
        
    def classify(self, inp):
        points = 0
        cond = {}
        if inp.get('gsr', 0) >= CFG["GSR_LEVEL_THRESH"]:
            points += 1
            cond['gsrLevel'] = True
        else:
            cond['gsrLevel'] = False
        if inp.get('gsr_slope', 0) >= CFG["GSR_SLOPE_THRESH"]:
            points += 1
            cond['gsrSlope'] = True
        else:
            cond['gsrSlope'] = False
        if inp.get('rmssd', 100) < CFG["RMSSD_LOW_THRESH_MS"] and inp.get('rmssd', 100) > 0:
            points += 1
            cond['hrvLow'] = True
        else:
            cond['hrvLow'] = False
        if inp.get('sound_sustained', False):
            points += 1
            cond['sound'] = True
        else:
            cond['sound'] = False
        
        moving = inp.get('motion', 0) >= CFG["MOTION_STILL_THRESH"]
        if points >= CFG["DISTRESS_MIN_POINTS"] and not moving:
            state = 'DISTRESS'
        elif points >= 1:
            state = 'ANXIOUS'
        else:
            state = 'CALM'
        return state, points, cond, moving
    
    def parse_line(self, line):
        line = line.strip()
        if '[button]' in line and 'caregiver event mark' in line:
            self.marks += 1
            self.events.append({'time': time.time(), 'type': 'MARK', 'msg': 'Caregiver event mark'})
            return None
        if '[band]' in line:
            try:
                # Parse: [band] state=CALM gsr=0.31 slope=0.02 sound=0.00(0.0s) mot=0.00 bpm=74 rmssd=41
                import re
                m = re.search(r'state=(\w+)\s+gsr=([-\d.]+)\s+slope=([-\d.]+)\s+sound=([-\d.]+)\(([\d.]+)s\)\s+mot=([-\d.]+)\s+bpm=([\d.]+)\s+rmssd=([-\d.]+)', line)
                if m:
                    state_reported = m.group(1)
                    data = {
                        'state': state_reported,
                        'gsr': float(m.group(2)),
                        'gsr_slope': float(m.group(3)),
                        'sound': float(m.group(4)),
                        'sound_sustained_time': float(m.group(5)),
                        'motion': float(m.group(6)),
                        'bpm': float(m.group(7)),
                        'rmssd': float(m.group(8)),
                        'sound_sustained': float(m.group(5)) >= 1.0
                    }
                    return data
            except Exception as e:
                pass
        return None
    
    def update_data(self, parsed_data):
        if not parsed_data:
            return
        current_time = time.time() - self.session_start
        self.data['time'].append(current_time)
        self.data['state'].append(parsed_data['state'])
        self.data['gsr'].append(parsed_data['gsr'])
        self.data['gsr_slope'].append(parsed_data['gsr_slope'])
        self.data['sound'].append(parsed_data['sound'])
        self.data['motion'].append(parsed_data['motion'])
        self.data['bpm'].append(parsed_data['bpm'])
        self.data['rmssd'].append(parsed_data['rmssd'])
        
        self.state_counts[parsed_data['state']] += 1
        
        if parsed_data['state'] == 'DISTRESS':
            if self.distress_start is None:
                self.distress_start = time.time()
                if self.alerts < self.state_counts['DISTRESS'] + 1:
                    self.alerts += 1
                    self.events.append({'time': time.time(), 'type': 'ALERT', 'msg': 'Alert fired (buzzer + vibration)'})
        else:
            self.distress_start = None
        
        if self.last_state != parsed_data['state']:
            self.events.append({'time': time.time(), 'type': parsed_data['state'], 'msg': f'State → {parsed_data["state"]}'})
            self.last_state = parsed_data['state']

def main():
    st.markdown('<h1 class="main-header">🩹 Empathy Band - Caregiver Dashboard</h1>', unsafe_allow_html=True)
    st.markdown("*Prototype - Not a medical device. Thresholds are untuned heuristics.*")
    
    # Initialize session state
    if 'dashboard' not in st.session_state:
        st.session_state.dashboard = EmpathyBandDashboard()
    if 'serial_connected' not in st.session_state:
        st.session_state.serial_connected = False
    if 'demo_mode' not in st.session_state:
        st.session_state.demo_mode = True
    
    dashboard = st.session_state.dashboard
    
    # Sidebar
    with st.sidebar:
        st.header("Connection Settings")
        connection_mode = st.radio("Mode", ["Demo Mode", "Serial Port"], index=0 if st.session_state.demo_mode else 1)
        
        if connection_mode == "Serial Port":
            st.session_state.demo_mode = False
            port = st.text_input("Serial Port", value="/dev/ttyUSB0", help="e.g., /dev/ttyUSB0, COM3")
            baud_rate = st.selectbox("Baud Rate", [115200, 9600, 57600], index=0)
            if st.button("Connect"):
                try:
                    ser = serial.Serial(port, baud_rate, timeout=1)
                    st.session_state.serial_port = ser
                    st.session_state.serial_connected = True
                    st.success(f"Connected to {port}")
                except Exception as e:
                    st.error(f"Failed to connect: {e}")
            if st.button("Disconnect"):
                if hasattr(st.session_state, 'serial_port'):
                    st.session_state.serial_port.close()
                st.session_state.serial_connected = False
                st.info("Disconnected")
        else:
            st.session_state.demo_mode = True
            st.session_state.serial_connected = False
            if st.button("Start Demo Simulation"):
                st.session_state.demo_running = True
        
        st.header("Session Info")
        session_duration = time.time() - dashboard.session_start
        st.metric("Session Time", f"{int(session_duration//60):02d}:{int(session_duration%60):02d}")
        st.metric("Events", len(dashboard.events))
        st.metric("Alerts Fired", dashboard.alerts)
        st.metric("Caregiver Marks", dashboard.marks)
    
    # Main content
    col1, col2, col3 = st.columns([2, 1, 1])
    
    with col1:
        st.subheader("Current State")
        if len(dashboard.data['state']) > 0:
            current_state = dashboard.data['state'][-1]
            if current_state == 'CALM':
                st.markdown(f'<div class="state-calm">CALM - Monitoring · baseline stable</div>', unsafe_allow_html=True)
            elif current_state == 'ANXIOUS':
                st.markdown(f'<div class="state-anxious">ANXIOUS - Elevated arousal · watch closely</div>', unsafe_allow_html=True)
            elif current_state == 'DISTRESS':
                st.markdown(f'<div class="state-distress">DISTRESS - Sustained distress · caregiver alerted</div>', unsafe_allow_html=True)
        else:
            st.info("Waiting for data...")
    
    with col2:
        st.subheader("Heart Rate")
        if len(dashboard.data['bpm']) > 0:
            st.metric("BPM", f"{dashboard.data['bpm'][-1]:.0f}")
        else:
            st.metric("BPM", "—")
    
    with col3:
        st.subheader("HRV (RMSSD)")
        if len(dashboard.data['rmssd']) > 0:
            st.metric("RMSSD", f"{dashboard.data['rmssd'][-1]:.0f} ms")
        else:
            st.metric("RMSSD", "—")
    
    # Sensor metrics
    st.subheader("Sensor Readings")
    col1, col2, col3, col4 = st.columns(4)
    
    with col1:
        if len(dashboard.data['gsr']) > 0:
            st.metric("GSR Level", f"{dashboard.data['gsr'][-1]:.3f}", help="Skin conductance (0-1)")
        else:
            st.metric("GSR Level", "—")
    
    with col2:
        if len(dashboard.data['gsr_slope']) > 0:
            st.metric("GSR Slope", f"{dashboard.data['gsr_slope'][-1]:.3f}/s", help="Rate of change")
        else:
            st.metric("GSR Slope", "—")
    
    with col3:
        if len(dashboard.data['sound']) > 0:
            st.metric("Sound Level", f"{dashboard.data['sound'][-1]:.3f}", help="Vocalization/ambient noise")
        else:
            st.metric("Sound Level", "—")
    
    with col4:
        if len(dashboard.data['motion']) > 0:
            st.metric("Motion", f"{dashboard.data['motion'][-1]:.3f} g", help="IMU magnitude deviation")
        else:
            st.metric("Motion", "—")
    
    # Charts
    st.subheader("Signal Timeline")
    if len(dashboard.data['time']) > 0:
        fig = make_subplots(rows=2, cols=1, shared_xaxes=True, 
                           vertical_spacing=0.1,
                           subplot_titles=('Sensor Values', 'State Timeline'),
                           row_heights=[0.7, 0.3])
        
        # Sensor traces
        fig.add_trace(go.Scatter(x=list(dashboard.data['time']), y=list(dashboard.data['gsr']),
                                mode='lines', name='GSR', line=dict(color='#22d3ee')), row=1, col=1)
        fig.add_trace(go.Scatter(x=list(dashboard.data['time']), y=list(dashboard.data['sound']),
                                mode='lines', name='Sound', line=dict(color='#a78bfa')), row=1, col=1)
        fig.add_trace(go.Scatter(x=list(dashboard.data['time']), y=[m*2 for m in dashboard.data['motion']],
                                mode='lines', name='Motion x2', line=dict(color='#94a3b8')), row=1, col=1)
        fig.add_trace(go.Scatter(x=list(dashboard.data['time']), y=[r/100 for r in dashboard.data['rmssd']],
                                mode='lines', name='HRV/100', line=dict(color='#34d399')), row=1, col=1)
        
        # State timeline
        state_colors = {'CALM': '#2f9cf4', 'ANXIOUS': '#f5a623', 'DISTRESS': '#ef4444'}
        for i, state in enumerate(dashboard.data['state']):
            fig.add_trace(go.Scatter(x=[dashboard.data['time'][i]], y=[0.5], 
                                    mode='markers', marker=dict(size=10, color=state_colors.get(state, 'gray')),
                                    showlegend=False), row=2, col=1)
        
        fig.update_layout(height=500, showlegend=True)
        fig.update_xaxes(title_text="Time (seconds)", row=2, col=1)
        fig.update_yaxes(title_text="Normalized Value", row=1, col=1)
        fig.update_yaxes(title_text="State", row=2, col=1, tickvals=[0.5], ticktext=[''])
        st.plotly_chart(fig, use_container_width=True)
    
    # Classification and events
    col1, col2 = st.columns(2)
    
    with col1:
        st.subheader("Classification Breakdown")
        if len(dashboard.data['gsr']) > 0:
            inp = {
                'gsr': dashboard.data['gsr'][-1],
                'gsr_slope': dashboard.data['gsr_slope'][-1],
                'rmssd': dashboard.data['rmssd'][-1],
                'sound_sustained': len(dashboard.data['time']) > 0 and False,  # Simplified
                'motion': dashboard.data['motion'][-1]
            }
            state_res, points, cond, moving = dashboard.classify(inp)
            
            st.write("**Conditions:**")
            st.write(f"{'✓' if cond.get('gsrLevel') else '✗'} GSR level elevated (≥{CFG['GSR_LEVEL_THRESH']})")
            st.write(f"{'✓' if cond.get('gsrSlope') else '✗'} GSR rising fast (≥{CFG['GSR_SLOPE_THRESH']}/s)")
            st.write(f"{'✓' if cond.get('hrvLow') else '✗'} HRV low (RMSSD < {CFG['RMSSD_LOW_THRESH_MS']}ms)")
            st.write(f"{'✓' if cond.get('sound') else '✗'} Sound sustained (≥{CFG['SOUND_THRESH']} for ≥{CFG['SOUND_SUSTAIN_MS']}ms)")
            
            st.write(f"**Points:** {points} / 4")
            st.write(f"**Motion:** {'MOVING' if moving else 'STILL'} {'(DISTRESS suppressed)' if moving else '(DISTRESS allowed)'}")
            st.write(f"**Result:** {state_res}")
    
    with col2:
        st.subheader("Event Log")
        if dashboard.events:
            for event in reversed(dashboard.events[-20:]):
                event_time = datetime.fromtimestamp(event['time']).strftime('%H:%M:%S')
                if event['type'] == 'DISTRESS':
                    st.error(f"{event_time} - {event['msg']}")
                elif event['type'] == 'ANXIOUS':
                    st.warning(f"{event_time} - {event['msg']}")
                elif event['type'] == 'CALM':
                    st.info(f"{event_time} - {event['msg']}")
                elif event['type'] == 'MARK':
                    st.success(f"{event_time} - {event['msg']}")
                elif event['type'] == 'ALERT':
                    st.error(f"🚨 {event_time} - {event['msg']}")
        else:
            st.info("No events yet")
    
    # Session summary
    st.subheader("Session Summary")
    col1, col2, col3 = st.columns(3)
    total_samples = sum(dashboard.state_counts.values())
    if total_samples > 0:
        with col1:
            calm_pct = (dashboard.state_counts['CALM'] / total_samples) * 100
            st.metric("Calm", f"{calm_pct:.1f}%", f"{dashboard.state_counts['CALM']} samples")
        with col2:
            anxious_pct = (dashboard.state_counts['ANXIOUS'] / total_samples) * 100
            st.metric("Anxious", f"{anxious_pct:.1f}%", f"{dashboard.state_counts['ANXIOUS']} samples")
        with col3:
            distress_pct = (dashboard.state_counts['DISTRESS'] / total_samples) * 100
            st.metric("Distress", f"{distress_pct:.1f}%", f"{dashboard.state_counts['DISTRESS']} samples")
    
    # Demo data generation
    if st.session_state.demo_mode and st.sidebar.button("Generate Demo Data", key="demo_gen"):
        # Generate sample data
        for i in range(10):
            t = time.time() - dashboard.session_start
            # Simulate some data
            stress = np.random.random()
            dashboard.data['time'].append(t + i)
            if stress > 0.7:
                state = 'DISTRESS'
            elif stress > 0.4:
                state = 'ANXIOUS'
            else:
                state = 'CALM'
            dashboard.data['state'].append(state)
            dashboard.data['gsr'].append(0.3 + stress * 0.4 + np.random.random() * 0.1)
            dashboard.data['gsr_slope'].append((stress - 0.3) * 0.2)
            dashboard.data['sound'].append(stress * 0.6)
            dashboard.data['motion'].append(np.random.random() * 0.05 if stress < 0.7 else np.random.random() * 0.3)
            dashboard.data['bpm'].append(70 + stress * 30)
            dashboard.data['rmssd'].append(50 - stress * 25)
            dashboard.state_counts[state] += 1
        st.rerun()
    
    # Auto-refresh for demo
    if st.session_state.demo_mode:
        time.sleep(0.1)
        st.rerun()

if __name__ == "__main__":
    main()
