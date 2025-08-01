#!/usr/bin/env python3
"""
VRAM System - Flask Server
Provides resource management API for M5StickC Plus2 dynamic memory system
"""

from flask import Flask, request, jsonify, send_file
import os
import json
import logging
import hashlib
import gzip
from datetime import datetime
import time
from resource_manager import ResourceManager

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('vram_server.log'),
        logging.StreamHandler()
    ]
)

app = Flask(__name__)
resource_manager = ResourceManager('resources/')

# Performance tracking
request_stats = {
    'total_requests': 0,
    'avg_response_time': 0,
    'failed_requests': 0
}

def track_performance(func):
    """Decorator to track API performance"""
    def wrapper(*args, **kwargs):
        start_time = time.time()
        try:
            result = func(*args, **kwargs)
            request_stats['total_requests'] += 1
            response_time = (time.time() - start_time) * 1000  # ms
            request_stats['avg_response_time'] = (
                (request_stats['avg_response_time'] * (request_stats['total_requests'] - 1) + response_time) 
                / request_stats['total_requests']
            )
            logging.info(f"API call {func.__name__} completed in {response_time:.2f}ms")
            return result
        except Exception as e:
            request_stats['failed_requests'] += 1
            logging.error(f"API call {func.__name__} failed: {str(e)}")
            return jsonify({'error': str(e)}), 500
    wrapper.__name__ = func.__name__
    return wrapper

@app.route('/api/health', methods=['GET'])
@track_performance
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat(),
        'stats': request_stats
    })

@app.route('/api/resources/<resource_id>', methods=['GET'])
@track_performance
def get_resource(resource_id):
    """
    Get a specific resource by ID
    Supports compression if requested
    """
    try:
        compress = request.args.get('compress', 'false').lower() == 'true'
        
        resource_data = resource_manager.get_resource(resource_id)
        if not resource_data:
            return jsonify({'error': 'Resource not found'}), 404
        
        # Log access
        resource_manager.log_access(resource_id, request.remote_addr)
        
        if compress and len(resource_data) > 512:  # Compress if > 512 bytes
            compressed_data = gzip.compress(resource_data)
            response = jsonify({
                'resource_id': resource_id,
                'data': compressed_data.hex(),  # Send as hex for JSON compatibility
                'compressed': True,
                'original_size': len(resource_data),
                'compressed_size': len(compressed_data),
                'timestamp': datetime.now().isoformat()
            })
        else:
            response = jsonify({
                'resource_id': resource_id,
                'data': resource_data.decode('utf-8') if isinstance(resource_data, bytes) else resource_data,
                'compressed': False,
                'size': len(resource_data),
                'timestamp': datetime.now().isoformat()
            })
        
        return response
        
    except Exception as e:
        logging.error(f"Error getting resource {resource_id}: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.route('/api/resources', methods=['GET'])
@track_performance
def list_resources():
    """
    List available resources with metadata
    Supports filtering and pagination
    """
    try:
        # Get query parameters
        category = request.args.get('category')
        max_size = request.args.get('max_size', type=int)
        page = request.args.get('page', 1, type=int)
        per_page = request.args.get('per_page', 50, type=int)
        
        resources = resource_manager.list_resources(
            category=category,
            max_size=max_size,
            page=page,
            per_page=per_page
        )
        
        return jsonify({
            'resources': resources,
            'total_count': len(resource_manager.get_all_resources()),
            'page': page,
            'per_page': per_page,
            'timestamp': datetime.now().isoformat()
        })
        
    except Exception as e:
        logging.error(f"Error listing resources: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.route('/api/resources/<resource_id>/version', methods=['GET'])
@track_performance
def check_version(resource_id):
    """
    Check resource version and metadata
    """
    try:
        version_info = resource_manager.get_version_info(resource_id)
        if not version_info:
            return jsonify({'error': 'Resource not found'}), 404
        
        return jsonify(version_info)
        
    except Exception as e:
        logging.error(f"Error checking version for {resource_id}: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.route('/api/resources', methods=['POST'])
@track_performance
def upload_resource():
    """
    Upload a new resource to the server
    """
    try:
        data = request.get_json()
        if not data or 'resource_id' not in data or 'content' not in data:
            return jsonify({'error': 'Missing required fields: resource_id, content'}), 400
        
        resource_id = data['resource_id']
        content = data['content']
        category = data.get('category', 'general')
        priority = data.get('priority', 1)
        
        success = resource_manager.store_resource(
            resource_id=resource_id,
            content=content,
            category=category,
            priority=priority
        )
        
        if success:
            return jsonify({
                'message': 'Resource uploaded successfully',
                'resource_id': resource_id,
                'timestamp': datetime.now().isoformat()
            }), 201
        else:
            return jsonify({'error': 'Failed to store resource'}), 500
            
    except Exception as e:
        logging.error(f"Error uploading resource: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.route('/api/resources/<resource_id>', methods=['DELETE'])
@track_performance
def delete_resource(resource_id):
    """
    Delete a resource from the server
    """
    try:
        success = resource_manager.delete_resource(resource_id)
        if success:
            return jsonify({
                'message': 'Resource deleted successfully',
                'resource_id': resource_id,
                'timestamp': datetime.now().isoformat()
            })
        else:
            return jsonify({'error': 'Resource not found'}), 404
            
    except Exception as e:
        logging.error(f"Error deleting resource {resource_id}: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.route('/api/stats', methods=['GET'])
@track_performance
def get_stats():
    """
    Get server and resource usage statistics
    """
    try:
        stats = resource_manager.get_usage_stats()
        stats.update(request_stats)
        
        return jsonify({
            'server_stats': stats,
            'timestamp': datetime.now().isoformat()
        })
        
    except Exception as e:
        logging.error(f"Error getting stats: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.route('/api/optimize', methods=['POST'])
@track_performance
def optimize_resources():
    """
    Trigger resource optimization and cleanup
    """
    try:
        data = request.get_json() or {}
        max_size = data.get('max_total_size')
        
        result = resource_manager.optimize_storage(max_size=max_size)
        
        return jsonify({
            'message': 'Optimization completed',
            'result': result,
            'timestamp': datetime.now().isoformat()
        })
        
    except Exception as e:
        logging.error(f"Error during optimization: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500

@app.errorhandler(404)
def not_found(error):
    return jsonify({'error': 'Endpoint not found'}), 404

@app.errorhandler(500)
def internal_error(error):
    return jsonify({'error': 'Internal server error'}), 500

if __name__ == '__main__':
    logging.info("Starting VRAM System Server...")
    logging.info(f"Resource directory: {resource_manager.resource_dir}")
    
    # Create initial demo resources
    resource_manager.create_demo_resources()
    
    # Run the server
    app.run(
        host='0.0.0.0',
        port=5000,
        debug=False,
        threaded=True
    )