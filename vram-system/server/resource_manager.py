#!/usr/bin/env python3
"""
VRAM System - Resource Manager
Handles resource storage, versioning, and optimization
"""

import os
import json
import hashlib
import shutil
import logging
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any
import pickle
import gzip

class ResourceManager:
    """
    Manages resources for the VRAM system including:
    - Resource storage and retrieval
    - Version management
    - Usage tracking and optimization
    - LRU-based cleanup
    """
    
    def __init__(self, resource_dir: str):
        self.resource_dir = os.path.abspath(resource_dir)
        self.metadata_file = os.path.join(self.resource_dir, 'metadata.json')
        self.access_log_file = os.path.join(self.resource_dir, 'access.log')
        
        # Create directory if it doesn't exist
        os.makedirs(self.resource_dir, exist_ok=True)
        
        # Load or create metadata
        self.metadata = self._load_metadata()
        
        logging.info(f"ResourceManager initialized with directory: {self.resource_dir}")
    
    def _load_metadata(self) -> Dict[str, Any]:
        """Load resource metadata from file"""
        if os.path.exists(self.metadata_file):
            try:
                with open(self.metadata_file, 'r') as f:
                    return json.load(f)
            except Exception as e:
                logging.error(f"Error loading metadata: {e}")
        
        return {
            'resources': {},
            'version': '1.0.0',
            'created': datetime.now().isoformat(),
            'last_updated': datetime.now().isoformat()
        }
    
    def _save_metadata(self):
        """Save metadata to file"""
        try:
            self.metadata['last_updated'] = datetime.now().isoformat()
            with open(self.metadata_file, 'w') as f:
                json.dump(self.metadata, f, indent=2)
        except Exception as e:
            logging.error(f"Error saving metadata: {e}")
    
    def _get_file_path(self, resource_id: str) -> str:
        """Get the file path for a resource"""
        return os.path.join(self.resource_dir, f"{resource_id}.dat")
    
    def _calculate_hash(self, data: bytes) -> str:
        """Calculate SHA256 hash of data"""
        return hashlib.sha256(data).hexdigest()
    
    def store_resource(self, resource_id: str, content: Any, category: str = 'general', priority: int = 1) -> bool:
        """
        Store a resource with metadata
        
        Args:
            resource_id: Unique identifier for the resource
            content: Resource content (string, bytes, or object)
            category: Resource category for organization
            priority: Priority level (1=critical, 2=important, 3=normal, 4=low)
        
        Returns:
            bool: True if successful, False otherwise
        """
        try:
            # Convert content to bytes
            if isinstance(content, str):
                data = content.encode('utf-8')
            elif isinstance(content, bytes):
                data = content
            else:
                # Serialize objects
                data = pickle.dumps(content)
            
            # Store the file
            file_path = self._get_file_path(resource_id)
            with open(file_path, 'wb') as f:
                f.write(data)
            
            # Update metadata
            self.metadata['resources'][resource_id] = {
                'size': len(data),
                'hash': self._calculate_hash(data),
                'category': category,
                'priority': priority,
                'created': datetime.now().isoformat(),
                'last_accessed': datetime.now().isoformat(),
                'access_count': 0,
                'version': 1
            }
            
            self._save_metadata()
            logging.info(f"Stored resource {resource_id} ({len(data)} bytes)")
            return True
            
        except Exception as e:
            logging.error(f"Error storing resource {resource_id}: {e}")
            return False
    
    def get_resource(self, resource_id: str) -> Optional[bytes]:
        """
        Retrieve a resource by ID
        
        Args:
            resource_id: The resource identifier
            
        Returns:
            bytes: Resource data or None if not found
        """
        try:
            if resource_id not in self.metadata['resources']:
                return None
            
            file_path = self._get_file_path(resource_id)
            if not os.path.exists(file_path):
                # Clean up orphaned metadata
                del self.metadata['resources'][resource_id]
                self._save_metadata()
                return None
            
            with open(file_path, 'rb') as f:
                data = f.read()
            
            # Update access information
            self.metadata['resources'][resource_id]['last_accessed'] = datetime.now().isoformat()
            self.metadata['resources'][resource_id]['access_count'] += 1
            self._save_metadata()
            
            return data
            
        except Exception as e:
            logging.error(f"Error retrieving resource {resource_id}: {e}")
            return None
    
    def delete_resource(self, resource_id: str) -> bool:
        """
        Delete a resource
        
        Args:
            resource_id: The resource identifier
            
        Returns:
            bool: True if successful, False otherwise
        """
        try:
            if resource_id not in self.metadata['resources']:
                return False
            
            file_path = self._get_file_path(resource_id)
            if os.path.exists(file_path):
                os.remove(file_path)
            
            del self.metadata['resources'][resource_id]
            self._save_metadata()
            
            logging.info(f"Deleted resource {resource_id}")
            return True
            
        except Exception as e:
            logging.error(f"Error deleting resource {resource_id}: {e}")
            return False
    
    def list_resources(self, category: Optional[str] = None, max_size: Optional[int] = None, 
                      page: int = 1, per_page: int = 50) -> List[Dict[str, Any]]:
        """
        List resources with optional filtering
        
        Args:
            category: Filter by category
            max_size: Maximum resource size in bytes
            page: Page number for pagination
            per_page: Resources per page
            
        Returns:
            List of resource metadata
        """
        resources = []
        
        for resource_id, metadata in self.metadata['resources'].items():
            # Apply filters
            if category and metadata.get('category') != category:
                continue
            if max_size and metadata.get('size', 0) > max_size:
                continue
            
            resource_info = {
                'resource_id': resource_id,
                'size': metadata.get('size', 0),
                'category': metadata.get('category', 'unknown'),
                'priority': metadata.get('priority', 3),
                'created': metadata.get('created'),
                'last_accessed': metadata.get('last_accessed'),
                'access_count': metadata.get('access_count', 0),
                'version': metadata.get('version', 1)
            }
            resources.append(resource_info)
        
        # Sort by last accessed (most recent first)
        resources.sort(key=lambda x: x['last_accessed'] or '1970-01-01', reverse=True)
        
        # Apply pagination
        start_idx = (page - 1) * per_page
        end_idx = start_idx + per_page
        
        return resources[start_idx:end_idx]
    
    def get_all_resources(self) -> Dict[str, Dict[str, Any]]:
        """Get all resource metadata"""
        return self.metadata['resources']
    
    def get_version_info(self, resource_id: str) -> Optional[Dict[str, Any]]:
        """
        Get version information for a resource
        
        Args:
            resource_id: The resource identifier
            
        Returns:
            Dictionary with version info or None if not found
        """
        if resource_id not in self.metadata['resources']:
            return None
        
        resource_meta = self.metadata['resources'][resource_id]
        return {
            'resource_id': resource_id,
            'version': resource_meta.get('version', 1),
            'hash': resource_meta.get('hash'),
            'size': resource_meta.get('size', 0),
            'last_modified': resource_meta.get('created'),
            'priority': resource_meta.get('priority', 3)
        }
    
    def log_access(self, resource_id: str, client_ip: str = 'unknown'):
        """Log resource access for analytics"""
        try:
            log_entry = {
                'timestamp': datetime.now().isoformat(),
                'resource_id': resource_id,
                'client_ip': client_ip
            }
            
            with open(self.access_log_file, 'a') as f:
                f.write(json.dumps(log_entry) + '\n')
                
        except Exception as e:
            logging.error(f"Error logging access: {e}")
    
    def get_usage_stats(self) -> Dict[str, Any]:
        """Get usage statistics"""
        total_resources = len(self.metadata['resources'])
        total_size = sum(meta.get('size', 0) for meta in self.metadata['resources'].values())
        
        # Category breakdown
        categories = {}
        for meta in self.metadata['resources'].values():
            cat = meta.get('category', 'unknown')
            if cat not in categories:
                categories[cat] = {'count': 0, 'size': 0}
            categories[cat]['count'] += 1
            categories[cat]['size'] += meta.get('size', 0)
        
        # Most accessed resources
        most_accessed = sorted(
            [(rid, meta.get('access_count', 0)) for rid, meta in self.metadata['resources'].items()],
            key=lambda x: x[1],
            reverse=True
        )[:10]
        
        return {
            'total_resources': total_resources,
            'total_size_bytes': total_size,
            'total_size_mb': round(total_size / (1024 * 1024), 2),
            'categories': categories,
            'most_accessed': most_accessed,
            'disk_usage': self._get_disk_usage()
        }
    
    def _get_disk_usage(self) -> Dict[str, int]:
        """Get disk usage information"""
        try:
            total, used, free = shutil.disk_usage(self.resource_dir)
            return {
                'total_bytes': total,
                'used_bytes': used,
                'free_bytes': free,
                'usage_percent': round((used / total) * 100, 2)
            }
        except Exception:
            return {'error': 'Unable to get disk usage'}
    
    def optimize_storage(self, max_size: Optional[int] = None) -> Dict[str, Any]:
        """
        Optimize storage by removing least recently used resources
        
        Args:
            max_size: Maximum total size to maintain in bytes
            
        Returns:
            Dictionary with optimization results
        """
        try:
            current_size = sum(meta.get('size', 0) for meta in self.metadata['resources'].values())
            
            if max_size is None:
                # Default: remove if total size > 100MB
                max_size = 100 * 1024 * 1024
            
            if current_size <= max_size:
                return {
                    'action': 'no_optimization_needed',
                    'current_size': current_size,
                    'max_size': max_size
                }
            
            # Sort resources by LRU algorithm considering priority
            resources_to_evaluate = []
            for resource_id, meta in self.metadata['resources'].items():
                priority = meta.get('priority', 3)
                last_accessed = datetime.fromisoformat(meta.get('last_accessed', '1970-01-01T00:00:00'))
                access_count = meta.get('access_count', 0)
                size = meta.get('size', 0)
                
                # Calculate score (lower = more likely to be deleted)
                # Critical resources (priority 1) have high scores
                age_days = (datetime.now() - last_accessed).days
                score = (access_count * 10) + (5 - priority) * 100 - age_days
                
                resources_to_evaluate.append((resource_id, score, size, priority))
            
            # Sort by score (ascending - delete lowest scores first)
            resources_to_evaluate.sort(key=lambda x: (x[3], x[1]))  # Priority first, then score
            
            deleted_resources = []
            freed_space = 0
            
            for resource_id, score, size, priority in resources_to_evaluate:
                if current_size - freed_space <= max_size:
                    break
                
                # Don't delete critical resources unless absolutely necessary
                if priority == 1 and (current_size - freed_space) / max_size < 1.5:
                    continue
                
                if self.delete_resource(resource_id):
                    deleted_resources.append({
                        'resource_id': resource_id,
                        'size': size,
                        'priority': priority,
                        'score': score
                    })
                    freed_space += size
            
            return {
                'action': 'optimization_completed',
                'deleted_count': len(deleted_resources),
                'freed_space_bytes': freed_space,
                'freed_space_mb': round(freed_space / (1024 * 1024), 2),
                'final_size_bytes': current_size - freed_space,
                'deleted_resources': deleted_resources
            }
            
        except Exception as e:
            logging.error(f"Error during optimization: {e}")
            return {'action': 'optimization_failed', 'error': str(e)}
    
    def create_demo_resources(self):
        """Create demo resources for testing"""
        try:
            demo_resources = [
                {
                    'id': 'config_main',
                    'content': '{"wifi_ssid":"demo_network","timeout":5000,"buffer_size":1024}',
                    'category': 'config',
                    'priority': 1
                },
                {
                    'id': 'lib_sensor',
                    'content': '#include <sensor.h>\nvoid readSensor() { /* sensor code */ }',
                    'category': 'library',
                    'priority': 2
                },
                {
                    'id': 'data_sample',
                    'content': 'Sample data for testing purposes. This could be a larger dataset.',
                    'category': 'data',
                    'priority': 3
                },
                {
                    'id': 'ui_strings',
                    'content': '{"welcome":"Welcome to VRAM","error":"Error occurred","loading":"Loading..."}',
                    'category': 'ui',
                    'priority': 2
                }
            ]
            
            for resource in demo_resources:
                if resource['id'] not in self.metadata['resources']:
                    self.store_resource(
                        resource['id'],
                        resource['content'],
                        resource['category'],
                        resource['priority']
                    )
            
            logging.info("Demo resources created")
            
        except Exception as e:
            logging.error(f"Error creating demo resources: {e}")