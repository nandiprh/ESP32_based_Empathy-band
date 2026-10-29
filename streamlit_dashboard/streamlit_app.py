import streamlit as st
import pandas as pd
import numpy as np
import time
from datetime import datetime
import plotly.graph_objects as go
from plotly.subplots import make_subplots

st.set_page_config(
    page_title="Empathy Band Dashboard",
    page_icon="❤️",
    layout="wide"
)

# Configuration
CFG = {
    "GSR_LEVEL": 0.55, "GSR_SLOPE": 0.08, "RMSSD_LOW": 30,
    "SOUND": 0.50, "SOUND_SUSTAIN_MS": 1000,
    "MOTION_STILL": 0.12, "DISTRESS_MIN": 3,
    "ALERT_DELAY_MS": 0
}

st.title("🩺 Empathy Band - Caregiver Dashboard")
st.caption("Real-time monitoring for Alzheimer's/Dementia care | Prototype - Not a medical device")

# Initialize state
if 'data' not in st.session_state:
    st.session_state.data = []
if 'events' not in st.session_state:
    st.session_state.events = []
if 'start_time' not in st.session_state:
    st.session_state.start_time = time.time()
if 'state_counts' not in st.session_state:
    st.session_state.state_counts = {'CALM': 0, 'ANXIOUS': 0, 'DISTRESS': 0}

# Sidebar
with st.sidebar:
    st.header("Controls")
    mode = st.radio("Mode", ["Demo", "Live Serial"])
    if st.button("Reset Session"):
        st.session_state.data = []
        st.session_state.events = []
        st.session_state.start_time = time.time()
        st.session_state.state_counts = {'CALM': 0, 'ANXIOUS': 0, 'DISTRESS': 0}
        st.rerun()
    st.header("Thresholds")
    st.text(f"GSR Level: {CFG['GSR_LEVEL']}")
    st.text(f"GSR Slope: {CFG['GSR_SLOPE']}")
    st.text(f"RMSSD Low: {CFG['RMSSD_LOW']} ms")
    st.text(f"Motion Gate: {CFG['MOTION_STILL']} g")

# Generate demo data
if mode == "Demo" and st.button("Simulate Episode"):
    t = time.time() - st.session_state.start_time
    # Simulate distress episode
    for i in range(30):
        stress = min(1.0, i/20.0)
        if stress < 0.3:
            state = 'CALM'
        elif stress < 0.7:
            state = 'ANXIOUS'
        else:
            state = 'DISTRESS'
        st.session_state.data.append({
            'time': t + i*0.1,
            'state': state,
            'gsr': 0.2 + stress*0.5 + np.random.normal(0,0.05),
            'gsr_slope': stress*0.15,
            'sound': stress*0.7,
            'motion': 0.05 if stress < 0.8 else 0.15,
            'bpm': 70 + stress*25,
            'rmssd': 45 - stress*20
        })
        st.session_state.state_counts[state] += 1
    st.session_state.events.append({'time': t, 'msg': 'State → DISTRESS', 'type': 'DISTRESS'})
    st.success("Simulated distress episode added")

# Display current status
if st.session_state.data:
    latest = st.session_state.data[-1]
    col1, col2, col3 = st.columns(3)
    with col1:
        if latest['state'] == 'CALM':
            st.success(f"### {latest['state']}\nMonitoring · baseline stable")
        elif latest['state'] == 'ANXIOUS':
            st.warning(f"### {latest['state']}\nElevated arousal · watch closely")
        else:
            st.error(f"### {latest['state']}\nSustained distress · caregiver alerted")
    with col2:
        st.metric("Heart Rate", f"{latest['bpm']:.0f} BPM")
        st.metric("HRV (RMSSD)", f"{latest['rmssd']:.0f} ms")
    with col3:
        st.metric("GSR Level", f"{latest['gsr']:.3f}")
        st.metric("Motion", f"{latest['motion']:.3f} g")

# Charts
if st.session_state.data:
    df = pd.DataFrame(st.session_state.data)
    fig = make_subplots(rows=2, cols=1, shared_xaxes=True, 
                       subplot_titles=("Physiological Signals", "Patient State"),
                       vertical_spacing=0.12)
    
    fig.add_trace(go.Scatter(x=df['time'], y=df['gsr'], name='GSR', line=dict(color='cyan')), row=1, col=1)
    fig.add_trace(go.Scatter(x=df['time'], y=df['sound'], name='Sound', line=dict(color='purple')), row=1, col=1)
    fig.add_trace(go.Scatter(x=df['time'], y=df['motion'], name='Motion', line=dict(color='gray')), row=1, col=1)
    
    colors = df['state'].map({'CALM': 'blue', 'ANXIOUS': 'orange', 'DISTRESS': 'red'})
    fig.add_trace(go.Scatter(x=df['time'], y=[1]*len(df), mode='markers', 
                            marker=dict(color=colors, size=12), name='State', showlegend=False), row=2, col=1)
    
    fig.update_layout(height=600, title_text="Empathy Band Monitoring")
    st.plotly_chart(fig, use_container_width=True)

# Session summary
st.header("Session Summary")
col1, col2, col3, col4 = st.columns(4)
total = sum(st.session_state.state_counts.values()) or 1
col1.metric("Calm", f"{st.session_state.state_counts['CALM']/total*100:.1f}%")
col2.metric("Anxious", f"{st.session_state.state_counts['ANXIOUS']/total*100:.1f}%")
col3.metric("Distress", f"{st.session_state.state_counts['DISTRESS']/total*100:.1f}%")
col4.metric("Alerts", len([e for e in st.session_state.events if e['type']=='DISTRESS']))

# Events
st.header("Event Log")
if st.session_state.events:
    for event in reversed(st.session_state.events[-10:]):
        st.write(f"⏱️ {event['msg']}")
else:
    st.info("No events recorded")
