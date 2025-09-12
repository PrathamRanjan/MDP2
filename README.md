# MDP 25/26 - Robot Path Planning Simulator

<div align="center">
  <h2>🤖 Multidisciplinary Design Project - Algorithm Simulator</h2>
  <p><em>Advanced Robot Path Planning with Hamiltonian Path Optimization</em></p>
  
  ![MDP](https://img.shields.io/badge/MDP-25%2F26-brightgreen)
  ![Status](https://img.shields.io/badge/Status-Production%20Ready-success)
  ![Tests](https://img.shields.io/badge/Tests-All%20Passing-brightgreen)
</div>

## 📋 Overview

This repository contains the complete implementation of the **CZ3004/SC2079 Multidisciplinary Design Project** robot path planning simulator for academic year 2025/26. The system provides a comprehensive solution for **B.1 Robot Movement Area Simulator**, **B.2 Hamiltonian Path Computation**, and **B.3 Shortest-time Path Optimization**.

## 🏗️ Architecture

```
MDP/
├── frontend/          # React/Next.js Web Interface
├── backend/           # Flask API & Path Planning Algorithm  
└── README.md         # This file
```

### **System Components:**
- **Frontend**: Modern React-based web simulator with real-time visualization
- **Backend**: Python Flask API with A* search and TSP optimization algorithms
- **Algorithm**: Advanced pathfinding with dynamic programming and safety constraints

## ✨ Features

### **🎯 B.1: Robot Movement Area Simulator**
- ✅ **2.0m × 2.0m Grid**: 20×20 cell arena with 10cm resolution
- ✅ **Real-time Visualization**: Live robot position and movement tracking
- ✅ **Interactive Controls**: Drag-and-drop obstacle placement
- ✅ **Step-by-step Navigation**: Forward/backward path execution
- ✅ **Start Zone Display**: Visual robot starting position

### **🔄 B.2: Hamiltonian Path Computation**
- ✅ **TSP Optimization**: Dynamic programming solution for visiting all obstacles
- ✅ **Image Recognition Integration**: SNAP commands for each obstacle
- ✅ **Constraint Handling**: Safe distance maintenance (3+ unit clearance)
- ✅ **Multiple Configurations**: Supports up to 5 obstacles simultaneously
- ✅ **Time Limit Compliance**: Configurable iteration limits

### **⚡ B.3: Shortest-time Hamiltonian Path**
- ✅ **A* Search Algorithm**: Optimal pathfinding between waypoints  
- ✅ **Turn Cost Optimization**: Smart direction change penalties
- ✅ **Viewing Position Selection**: Multiple candidates per obstacle
- ✅ **Safety Penalties**: Risk assessment for obstacle proximity
- ✅ **Retrying Mode**: Extended optimization for time-critical scenarios

## 🚀 Quick Start

### **Prerequisites**
- Node.js 16+ and npm
- Python 3.8+ with pip
- Modern web browser

### **Installation & Setup**

1. **Clone Repository**
   ```bash
   git clone https://github.com/PrathamRanjan/MDP2.git
   cd MDP2
   ```

2. **Backend Setup**
   ```bash
   cd backend
   pip install -r requirements.txt
   python main.py
   ```
   Backend runs on: `http://localhost:5001`

3. **Frontend Setup**
   ```bash
   cd frontend
   npm install
   npm run dev
   ```
   Frontend runs on: `http://localhost:3000`

### **Usage**

1. **Access Simulator**: Open `http://localhost:3000`
2. **Set Robot Position**: Use X, Y, Direction controls
3. **Add Obstacles**: Click coordinates and select direction
4. **Run Algorithm**: Click "Submit" to compute optimal path
5. **Visualize Path**: Use navigation arrows to step through execution

## 🧪 Testing Requirements

### **B.1 Tests**
- Grid display accuracy and robot positioning
- Real-time movement visualization
- Edge case handling (corners, boundaries)

### **B.2 Tests**  
- Single obstacle traversal
- 5-obstacle Hamiltonian path computation
- Skip obstacles (direction = None) handling

### **B.3 Tests**
- Path distance optimization comparison
- Turn cost minimization verification
- Retrying mode performance analysis

## 📊 Performance Metrics

| Metric | Specification | Achieved |
|--------|--------------|----------|
| **Grid Size** | 20×20 cells | ✅ Implemented |
| **Max Obstacles** | 5 obstacles | ✅ Supported |
| **Response Time** | < 2 seconds | ✅ Sub-second |
| **Path Optimality** | TSP-optimal | ✅ Dynamic Programming |
| **Safety Clearance** | 3+ units | ✅ Enforced |

## 🛠️ Technical Specifications

### **Frontend Stack**
- **Framework**: React 18.2.0 + Next.js 13.3.1
- **Styling**: TailwindCSS + DaisyUI
- **Build Tool**: Next.js with hot reload

### **Backend Stack**
- **Framework**: Flask with CORS support
- **Algorithm**: A* search + TSP dynamic programming
- **Libraries**: NumPy, python-tsp, OpenCV
- **Architecture**: RESTful API design

### **API Endpoints**
- `GET /status` - Health check
- `POST /path` - Path planning computation
- `POST /image` - Image recognition (future)
- `GET /stitch` - Image stitching (future)

## 🎨 UI Features

- **Modern Dark Theme**: Gradient backgrounds with neon accents
- **Responsive Design**: Works on desktop and tablet devices  
- **Real-time Updates**: Live path visualization and status
- **Interactive Grid**: Click-to-place obstacles and robot
- **Command Display**: Shows robot movement commands (FW, BR, SNAP, etc.)

## 📝 Algorithm Details

### **Path Planning Pipeline**
1. **Grid Initialization**: 20×20 arena with obstacle mapping
2. **Viewpoint Generation**: Calculate optimal viewing positions for each obstacle
3. **TSP Optimization**: Find shortest Hamiltonian path visiting all viewpoints
4. **A* Pathfinding**: Compute optimal routes between consecutive waypoints
5. **Command Generation**: Convert path to robot movement instructions

### **Safety Constraints**
- Minimum 3-unit Manhattan distance from obstacles
- Turn radius validation for direction changes
- Boundary checking for all robot positions
- Collision avoidance during path execution

## 🔧 Configuration

Key parameters in `backend/consts.py`:
```python
WIDTH = 20              # Arena width (cells)
HEIGHT = 20             # Arena height (cells)  
ITERATIONS = 2000       # TSP optimization iterations
TURN_FACTOR = 1         # Turn cost multiplier
SAFE_COST = 1000       # Obstacle proximity penalty
```

## 🤝 Contributing

This is an academic project for MDP 25/26. For improvements or bug fixes:

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📜 License

This project is developed for academic purposes as part of the CZ3004/SC2079 Multidisciplinary Design Project course.

## 🏆 Acknowledgments

- **Base Algorithm**: Enhanced from previous MDP implementations
- **TSP Solver**: python-tsp library for dynamic programming optimization
- **UI Framework**: TailwindCSS and DaisyUI for modern interface design
- **Pathfinding**: A* algorithm with custom heuristics

---

<div align="center">
  <strong>🤖 Built for MDP 25/26 | Advanced Robot Path Planning 🚀</strong>
</div>