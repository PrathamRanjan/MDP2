# 🧠 MDP Backend - Advanced Path Planning Algorithm

<div align="center">
  <h2>Flask API with A* Search & TSP Optimization</h2>
  <p><em>High-performance robot path planning with Hamiltonian optimization</em></p>
  
  ![Python](https://img.shields.io/badge/Python-3.8+-blue)
  ![Flask](https://img.shields.io/badge/Flask-3.0.3-green)
  ![Algorithm](https://img.shields.io/badge/Algorithm-A*%20+%20TSP-red)
</div>

## 📋 Overview

The backend provides a robust Flask-based API for robot path planning, implementing advanced algorithms including **A* search**, **TSP dynamic programming**, and **safety-constrained navigation**. It serves as the computational engine for the MDP 25/26 robot path planning simulator.

## 🏗️ Architecture

```
backend/
├── algo/
│   └── algo.py          # Core MazeSolver algorithm
├── entities/
│   ├── Entity.py        # Grid, Obstacle, CellState classes
│   └── Robot.py         # Robot state management
├── models/              # YOLO object detection models
├── utils/               # YOLOv5 utilities and helpers
├── main.py             # Flask application entry point
├── consts.py           # Algorithm constants and parameters
├── helper.py           # Command generation utilities
├── model.py            # Image recognition integration
└── requirements.txt    # Python dependencies
```

## ✨ Core Features

### **🎯 Advanced Path Planning**
- **A* Search Algorithm**: Optimal pathfinding with heuristic guidance
- **TSP Dynamic Programming**: Shortest Hamiltonian path computation
- **Safety Constraints**: Maintains 3+ unit clearance from obstacles
- **Turn Optimization**: Smart direction change cost minimization

### **🔄 Hamiltonian Path Solver**
- **Visit All Obstacles**: Guarantees each obstacle visited exactly once
- **Multiple Viewing Angles**: 2-4 optimal positions per obstacle
- **Penalty System**: Distance and angle penalties for path optimization
- **Time Limit Compliance**: Configurable iteration limits (2000 default)

### **⚡ Performance Optimizations**
- **Memoization**: Path cost caching for repeated calculations
- **Iterative Deepening**: Configurable search depth limits  
- **Safe Distance Caching**: Pre-computed obstacle proximity costs
- **View Position Filtering**: Only reachable positions considered

## 🚀 Quick Start

### **Prerequisites**
- Python 3.8 or higher
- pip package manager

### **Installation**
```bash
# Install dependencies
pip install -r requirements.txt

# Start Flask server
python main.py
```

The API server will be available at `http://localhost:5001`

## 📡 API Endpoints

### **Health Check**
```http
GET /status
```
**Response:**
```json
{
  "result": "ok"
}
```

### **Path Planning**
```http
POST /path
Content-Type: application/json
```

**Request Body:**
```json
{
  "obstacles": [
    {"x": 10, "y": 10, "id": 1, "d": 0}  // North-facing obstacle
  ],
  "robot_x": 1,           // Robot X coordinate (1-18)
  "robot_y": 1,           // Robot Y coordinate (1-18)  
  "robot_dir": 0,         // Direction: North=0, East=2, South=4, West=6
  "retrying": false       // Enable extended optimization
}
```

**Response:**
```json
{
  "data": {
    "commands": ["FW90", "SNAP1_C", "FIN"],
    "distance": 46.0,
    "path": [
      {"x": 1, "y": 1, "d": 0, "s": -1},
      {"x": 10, "y": 14, "d": 4, "s": 1}
    ]
  },
  "error": null
}
```

### **Future Endpoints**
- `POST /image` - Image recognition processing
- `GET /stitch` - Image stitching functionality

## 🧮 Algorithm Details

### **MazeSolver Class Architecture**

```python
class MazeSolver:
    def __init__(self, size_x, size_y, robot_x, robot_y, robot_direction, big_turn=None)
    def add_obstacle(self, x, y, direction, obstacle_id)
    def get_optimal_order_dp(self, retrying) -> List[CellState]
    def path_cost_generator(self, states)
    def get_neighbors(self, x, y, direction)
```

### **Path Planning Pipeline**

1. **Initialization**
   - Create 20×20 grid environment
   - Place robot at starting position
   - Add obstacles with directional symbols

2. **Viewpoint Generation**
   - Calculate optimal viewing positions for each obstacle
   - Filter positions for safety constraints
   - Apply distance and angle penalties

3. **TSP Optimization**
   - Generate all possible visit combinations
   - Use dynamic programming to find shortest Hamiltonian path
   - Consider turn costs and safety penalties

4. **A* Pathfinding**
   - Compute optimal routes between consecutive waypoints
   - Apply heuristic distance calculations
   - Respect turn radius and safety constraints

5. **Command Generation**
   - Convert path waypoints to robot movement commands
   - Generate SNAP commands for image capture
   - Add FIN termination command

### **Safety Constraint System**

```python
def get_safe_cost(self, x, y):
    """Calculate safety penalty for position proximity to obstacles"""
    for obstacle in obstacles:
        if abs(obstacle.x - x) <= 2 and abs(obstacle.y - y) <= 2:
            return SAFE_COST  # 1000 point penalty
    return 0
```

**Safety Rules:**
- Minimum 2-unit clearance during movement
- 3-unit clearance for turns and pre-turn positions
- Manhattan distance ≥ 4 units total separation
- Maximum distance ≥ 3 units in any single direction

## 🔧 Configuration Parameters

### **Algorithm Constants (consts.py)**
```python
# Grid Configuration
WIDTH = 20               # Arena width in cells
HEIGHT = 20              # Arena height in cells
EXPANDED_CELL = 1        # Cell expansion factor

# Algorithm Parameters  
ITERATIONS = 2000        # TSP optimization iterations
TURN_FACTOR = 1          # Turn cost multiplier
TURN_RADIUS = 1          # Robot turning radius

# Cost Penalties
SAFE_COST = 1000        # Obstacle proximity penalty
SCREENSHOT_COST = 50    # Non-optimal viewing angle penalty
```

### **Movement Directions**
```python
MOVE_DIRECTION = [
    (1, 0, Direction.EAST),   # Move East
    (-1, 0, Direction.WEST),  # Move West  
    (0, 1, Direction.NORTH),  # Move North
    (0, -1, Direction.SOUTH)  # Move South
]
```

## 🏁 Robot Movement Commands

### **Generated Command Types**
- **FW{distance}**: Move forward (e.g., FW90 = 9.0 units)
- **BW{distance}**: Move backward 
- **FR00/FL00**: Turn right/left 90°
- **BR00/BL00**: Reverse turn right/left 90°
- **SNAP{id}_C**: Capture image at obstacle {id}
- **FIN**: Path completion signal

### **Command Generation Logic**
```python
def command_generator(optimal_path, obstacles):
    """Convert path waypoints to robot movement commands"""
    commands = []
    for i, state in enumerate(optimal_path):
        if state.screenshot_id != -1:
            commands.append(f"SNAP{state.screenshot_id}_C")
        # Add movement commands based on position changes
    commands.append("FIN")
    return commands
```

## 🧪 Testing & Validation

### **Unit Testing**
```bash
# Test core algorithm components
python -m pytest tests/

# Test specific functions
python -c "from algo.algo import MazeSolver; print('Algorithm imported successfully')"
```

### **Performance Testing**
```bash
# Benchmark with 5 obstacles
curl -X POST http://localhost:5001/path \
  -H "Content-Type: application/json" \
  -d '{"obstacles": [/* 5 obstacles */], "robot_x": 1, "robot_y": 1, "robot_dir": 0, "retrying": false}'
```

### **Edge Case Testing**
- **No Obstacles**: Empty obstacle array
- **Skip Obstacles**: Direction = 8 (SKIP) obstacles
- **Boundary Conditions**: Robot/obstacles at grid edges
- **Invalid Positions**: Out-of-bounds coordinates

## 📊 Performance Metrics

| Scenario | Response Time | Path Optimality | Memory Usage |
|----------|---------------|-----------------|--------------|
| 1 Obstacle | < 100ms | Optimal | ~10MB |
| 3 Obstacles | < 500ms | Near-optimal | ~20MB |
| 5 Obstacles | < 2000ms | TSP-optimal | ~50MB |

## 🛠️ Dependencies

### **Core Libraries**
```python
flask==3.0.3           # Web framework
flask-cors==5.0.1      # Cross-origin resource sharing
numpy>=1.18.5          # Numerical computing
python-tsp==0.5.0      # TSP optimization
```

### **Image Processing (Future)**
```python
opencv-python>=4.1.2   # Computer vision
torch>=1.7.0           # Deep learning framework
torchvision>=0.8.1     # Vision utilities
```

### **Utilities**
```python
matplotlib>=3.2.2      # Plotting and visualization
scipy>=1.4.1           # Scientific computing
requests>=2.23.0       # HTTP client
```

## 🔍 Troubleshooting

### **Common Issues**

**ModuleNotFoundError: python_tsp**
```bash
pip install python-tsp
```

**Port 5000 Already in Use**
```python
# In main.py, change:
app.run(host='0.0.0.0', port=5001, debug=True)
```

**CORS Errors in Browser**
```python
# Verify CORS is enabled:
from flask_cors import CORS
CORS(app)
```

**Slow Path Computation**
```python
# Reduce iterations in consts.py:
ITERATIONS = 1000  # Default: 2000
```

## 🚀 Deployment

### **Production Configuration**
```python
# Disable debug mode
app.run(host='0.0.0.0', port=5001, debug=False)

# Use production WSGI server
gunicorn main:app --bind 0.0.0.0:5001
```

### **Docker Deployment**
```dockerfile
FROM python:3.8-slim
COPY requirements.txt .
RUN pip install -r requirements.txt
COPY . .
EXPOSE 5001
CMD ["python", "main.py"]
```

## 🔬 Advanced Features

### **Retrying Mode Optimization**
When `retrying: true` is enabled:
- Extended viewing position candidates (4-5 positions per obstacle)
- Longer distance thresholds for better optimization
- Additional penalty considerations for time-critical scenarios

### **Turn Radius Optimization**
```python
turn_wrt_big_turns = [
    [3 * TURN_RADIUS, TURN_RADIUS],     # 3-1 turn pattern
    [4 * TURN_RADIUS, 2 * TURN_RADIUS]  # 4-2 turn pattern  
]
```

### **Dynamic Programming Memoization**
- **Path Table**: Cached optimal paths between state pairs
- **Cost Table**: Stored minimum costs for repeated calculations
- **Memory Efficiency**: Automatic cleanup of unused cache entries

## 🤝 Contributing

When extending the backend:

1. **Algorithm Changes**: Modify `algo/algo.py` with comprehensive testing
2. **New Endpoints**: Add routes in `main.py` with proper error handling
3. **Parameter Tuning**: Update `consts.py` with performance validation
4. **Testing**: Include unit tests for all new functionality

## 📝 Future Enhancements

- **Multi-robot Support**: Parallel path planning for multiple robots
- **Dynamic Obstacles**: Real-time obstacle movement handling
- **3D Path Planning**: Extension to three-dimensional environments
- **Machine Learning Integration**: Neural network path optimization
- **Real-time Communication**: WebSocket support for live updates

---

<div align="center">
  <strong>🧠 Advanced Algorithm Engine for Robot Path Planning Excellence 🚀</strong>
</div>