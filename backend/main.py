import time
from algo.algo import MazeSolver 
from flask import Flask, request, jsonify
from flask_cors import CORS
from model import *
from algo.helper import command_generator, to_low_level_commands

app = Flask(__name__)
CORS(app)
#model = load_model()
model = None
@app.route('/status', methods=['GET'])
def status():
    """
    This is a health check endpoint to check if the server is running
    :return: a json object with a key "result" and value "ok"
    """
    return jsonify({"result": "ok"})


@app.route('/path', methods=['POST'])
def path_finding():
    """
    This is the main endpoint for the path finding algorithm
    :return: a json object with a key "data" and value a dictionary with keys "distance", "path", and "commands"
    """
    # Get the json data from the request
    content = request.json

    # Get the obstacles, big_turn, retrying, robot_x, robot_y, and robot_direction from the json data
    obstacles = content['obstacles']
    # big_turn = int(content['big_turn'])
    retrying = content['retrying']
    robot_x, robot_y = content['robot_x'], content['robot_y']
    robot_direction = int(content['robot_dir'])

    # Initialize MazeSolver object with robot size of 20x20, bottom left corner of robot at (1,1), facing north, and whether to use a big turn or not.
    maze_solver = MazeSolver(20, 20, robot_x, robot_y, robot_direction, big_turn=None)

    # Add each obstacle into the MazeSolver. Each obstacle is defined by its x,y positions, its direction, and its id
    for ob in obstacles:
        maze_solver.add_obstacle(ob['x'], ob['y'], ob['d'], ob['id'])

    start = time.time()
    # Get shortest path
    optimal_path, distance = maze_solver.get_optimal_order_dp(retrying=retrying)
    print(f"Time taken to find shortest path using A* search: {time.time() - start}s")
    print(f"Distance to travel: {distance} units")
    
    # Based on the shortest path, generate commands for the robot
    commands = command_generator(optimal_path, obstacles)
    low_level_commands = to_low_level_commands(commands)

    import requests
    from requests.adapters import HTTPAdapter
    from urllib3.util.retry import Retry

    # RPi command server configuration
    ROBOT_SERVER_URL = "http://10.100.110.132:5000/receive_command"  # Replace with your RPi's IP and port
    SEND_TO_ROBOT = True  # Set to False to disable sending commands to robot
    
    if SEND_TO_ROBOT:
        print(f"\n=== Sending commands to robot at {ROBOT_SERVER_URL} ===")
        
        # Test connection first
        try:
            test_response = requests.get("http://10.100.110.132:5000/status", timeout=5)
            if test_response.status_code == 200:
                print("✓ RPi connection test successful")
            else:
                print(f"⚠ RPi responded with status {test_response.status_code}")
        except Exception as e:
            print(f"✗ RPi connection test failed: {e}")
            print("  Continuing anyway...")
        
        # Configure requests session with retry strategy
        session = requests.Session()
        retry_strategy = Retry(
            total=3,
            backoff_factor=1,
            status_forcelist=[429, 500, 502, 503, 504],
        )
        adapter = HTTPAdapter(max_retries=retry_strategy)
        session.mount("http://", adapter)
        session.mount("https://", adapter)
        
        success_count = 0
        
        for i, cmd in enumerate(low_level_commands, 1):
            try:
                response = session.post(
                    ROBOT_SERVER_URL, 
                    json={"command": cmd.strip()}, 
                    timeout=(5, 10),  # (connect timeout, read timeout)
                    headers={'Connection': 'close'}  # Don't keep connections alive
                )
                print(f"[{i}/{len(low_level_commands)}] Sent: {cmd.strip()} | Response: {response.status_code}")
                if response.status_code == 200:
                    success_count += 1
                else:
                    print(f"  Warning: Command failed with status {response.status_code}")
            except requests.exceptions.ConnectionError as e:
                print(f"[{i}/{len(low_level_commands)}] Failed to send '{cmd.strip()}': Connection error")
                print(f"  Error details: {str(e)}")
                print("  Make sure rpi_command_server.py is running on the RPi")
            except requests.exceptions.Timeout as e:
                print(f"[{i}/{len(low_level_commands)}] Failed to send '{cmd.strip()}': Timeout")
                print(f"  Error details: {str(e)}")
            except Exception as e:
                print(f"[{i}/{len(low_level_commands)}] Failed to send '{cmd.strip()}': {type(e).__name__}")
                print(f"  Error details: {str(e)}")
        
        session.close()
        print(f"Commands sent successfully: {success_count}/{len(low_level_commands)}")
    else:
        print("\n=== Robot command sending is disabled ===")
        print("Set SEND_TO_ROBOT = True to enable sending commands to robot")

    # Log high-level and low-level commands clearly
    print("\n=== High-level commands ===")
    for cmd in commands:
        print(cmd)
    print("\n=== Low-level commands ===")
    for llcmd in low_level_commands:
        print(llcmd, end='')

    # Get the starting location and add it to path_results
    path_results = [optimal_path[0].get_dict()]
    # Process each command individually and append the location the robot should be after executing that command to path_results
    i = 0
    for command in commands:
        if command.startswith("SNAP"):
            continue
        if command.startswith("FIN"):
            continue
        elif command.startswith("FW") or command.startswith("FS"):
            i += int(command[2:]) // 10
        elif command.startswith("BW") or command.startswith("BS"):
            i += int(command[2:]) // 10
        else:
            i += 1
        path_results.append(optimal_path[i].get_dict())
    return jsonify({
        "data": {
            'distance': distance,
            'path': path_results,
            'commands': commands
        },
        "error": None
    })


@app.route('/image', methods=['POST'])
def image_predict():
    """
    This is the main endpoint for the image prediction algorithm
    :return: a json object with a key "result" and value a dictionary with keys "obstacle_id" and "image_id"
    """
    file = request.files['file']
    filename = file.filename
    file.save(os.path.join('uploads', filename))
    # filename format: "<timestamp>_<obstacle_id>_<signal>.jpeg"
    constituents = file.filename.split("_")
    obstacle_id = constituents[1]

    ## Week 8 ## 
    #signal = constituents[2].strip(".jpg")
    #image_id = predict_image(filename, model, signal)

    ## Week 9 ## 
    # We don't need to pass in the signal anymore
    image_id = predict_image_week_9(filename,model)

    # Return the obstacle_id and image_id
    result = {
        "obstacle_id": obstacle_id,
        "image_id": image_id
    }
    return jsonify(result)

@app.route('/stitch', methods=['GET'])
def stitch():
    """
    This is the main endpoint for the stitching command. Stitches the images using two different functions, in effect creating two stitches, just for redundancy purposes
    """
    img = stitch_image()
    img.show()
    img2 = stitch_image_own()
    img2.show()
    return jsonify({"result": "ok"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=True)
