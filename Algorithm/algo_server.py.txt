#!/usr/bin/env python3
"""
MDP Algorithm Server - Real Implementation
Uses the actual MazeSolver algorithm from backend
"""

from flask import Flask, request, jsonify
import logging
import json
import sys
import os
from typing import Dict, List

# Add backend directory to path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'backend'))

from algo.algo import MazeSolver
from entities.Entity import Obstacle, CellState
from entities.Robot import Robot
from algo.consts import Direction
from algo.helper import command_generator

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)

def create_maze_solver(robot_data: Dict, obstacles: List[Dict]) -> MazeSolver:
    """Create and configure MazeSolver with robot and obstacles"""
    
    # Extract robot position and direction
    robot_x = robot_data.get('x', 1)
    robot_y = robot_data.get('y', 1)
    robot_direction_str = robot_data.get('direction', 'NORTH')
    
    # Convert direction string to Direction enum
    direction_map = {
        'N': Direction.NORTH, 'NORTH': Direction.NORTH,
        'E': Direction.EAST, 'EAST': Direction.EAST,
        'S': Direction.SOUTH, 'SOUTH': Direction.SOUTH,
        'W': Direction.WEST, 'WEST': Direction.WEST
    }
    robot_direction = direction_map.get(robot_direction_str.upper(), Direction.NORTH)
    
    logger.info(f"Creating MazeSolver: robot at ({robot_x}, {robot_y}) facing {robot_direction.name}")
    
    # Initialize MazeSolver (20x20 grid is default)
    solver = MazeSolver(20, 20, robot_x, robot_y, robot_direction)
    
    # Add obstacles to solver
    for obs in obstacles:
        obs_x = obs.get('x')
        obs_y = obs.get('y')
        obs_direction_str = obs.get('direction', 'NORTH')
        obs_id = obs.get('id', 1)
        
        # Convert obstacle direction
        obs_direction = direction_map.get(obs_direction_str.upper(), Direction.NORTH)
        
        logger.info(f"Adding obstacle {obs_id} at ({obs_x}, {obs_y}) facing {obs_direction.name}")
        solver.add_obstacle(obs_x, obs_y, obs_direction, obs_id)
    
    return solver

def solve_path(solver: MazeSolver, retrying: bool = False) -> tuple:
    """Solve for optimal path using the MazeSolver"""
    try:
        logger.info(f"Computing optimal path (retrying={retrying})")
        path, distance = solver.get_optimal_order_dp(retrying)
        
        if not path:
            logger.warning("No path found, trying with retrying=True")
            if not retrying:
                return solve_path(solver, True)
            else:
                return [], float('inf')
        
        logger.info(f"Found path with {len(path)} states, distance: {distance}")
        return path, distance
        
    except Exception as e:
        logger.error(f"Error in path solving: {e}")
        return [], float('inf')

@app.route('/api/status', methods=['GET'])
def get_status():
    """Health check endpoint"""
    return jsonify({
        "status": "online",
        "service": "MDP Algorithm Server",
        "version": "2.0.0"
    })

@app.route('/api/path', methods=['POST'])
def request_path():
    """Main pathfinding endpoint using real MazeSolver algorithm"""
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({"error": "No data provided"}), 400
        
        obstacles = data.get('obstacles', [])
        robot_start = data.get('robot_start', {"x": 1, "y": 1, "direction": "NORTH"})
        task = data.get('task', 'path_planning')
        
        logger.info(f"Received pathfinding request:")
        logger.info(f"  - Obstacles: {len(obstacles)}")
        logger.info(f"  - Start position: {robot_start}")
        logger.info(f"  - Task: {task}")
        
        # Log obstacle details
        for i, obs in enumerate(obstacles):
            logger.info(f"  - Obstacle {i+1}: x={obs.get('x')}, y={obs.get('y')}, "
                       f"dir={obs.get('direction')}, id={obs.get('id')}")
        
        # Create and configure MazeSolver
        solver = create_maze_solver(robot_start, obstacles)
        
        # Solve for optimal path
        path_states, distance = solve_path(solver)
        
        if not path_states:
            return jsonify({"error": "No valid path found"}), 404
        
        # Convert path states to command format
        path_data = []
        for state in path_states:
            path_data.append({
                'x': state.x,
                'y': state.y,
                'direction': state.direction.name,
                'screenshot_id': state.screenshot_id if state.screenshot_id != -1 else None
            })
        
        # Generate movement commands using helper function
        obstacles_dict = [{'x': obs['x'], 'y': obs['y'], 'd': obs['direction'], 'id': obs['id']} 
                         for obs in obstacles]
        commands = command_generator(path_states, obstacles_dict)
        
        response = {
            "status": "success",
            "path": path_data,
            "commands": commands,
            "distance": distance,
            "total_commands": len(commands),
            "estimated_time": len(commands) * 2  # 2 seconds per command estimate
        }
        
        logger.info(f"Sending response with {len(path_data)} path states and {len(commands)} commands")
        return jsonify(response)
        
    except Exception as e:
        logger.error(f"Error in pathfinding request: {e}")
        return jsonify({"error": str(e)}), 500

@app.route('/api/image', methods=['POST'])
def process_image():
    """Process image data (placeholder for future image processing)"""
    try:
        position = request.form.get('position')
        image_file = request.files.get('image')
        
        logger.info(f"Received image at position: {position}")
        
        # Placeholder response - implement actual image processing here
        return jsonify({
            "status": "success",
            "detected_symbol": "A",  # Placeholder
            "confidence": 0.95,
            "position": position
        })
        
    except Exception as e:
        logger.error(f"Error processing image: {e}")
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    logger.info("Starting MDP Algorithm Server...")
    logger.info("Endpoints available:")
    logger.info("  GET  /api/status - Health check")
    logger.info("  POST /api/path   - Path planning with obstacles")
    logger.info("  POST /api/image  - Image processing")
    
    app.run(host='0.0.0.0', port=5002, debug=False)
