/**
 * MapView Component
 *
 * Main map component with Leaflet integration
 */

import React from 'react';
import { MapContainer, TileLayer, useMap, useMapEvents } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

// Fix Leaflet default icon issue with Webpack
import icon from 'leaflet/dist/images/marker-icon.png';
import iconShadow from 'leaflet/dist/images/marker-shadow.png';

let DefaultIcon = L.icon({
  iconUrl: icon,
  shadowUrl: iconShadow,
  iconSize: [25, 41],
  iconAnchor: [12, 41],
});

L.Marker.prototype.options.icon = DefaultIcon;

interface MapViewProps {
  onViewportChange?: (bounds: L.LatLngBounds, zoom: number) => void;
  children?: React.ReactNode;
}

/**
 * Map event handler component
 */
function MapEventHandler({
  onViewportChange,
}: {
  onViewportChange?: (bounds: L.LatLngBounds, zoom: number) => void;
}) {
  const map = useMap();

  useMapEvents({
    moveend: () => {
      if (onViewportChange) {
        const bounds = map.getBounds();
        const zoom = map.getZoom();
        onViewportChange(bounds, zoom);
      }
    },
    zoomend: () => {
      if (onViewportChange) {
        const bounds = map.getBounds();
        const zoom = map.getZoom();
        onViewportChange(bounds, zoom);
      }
    },
  });

  return null;
}

export const MapView: React.FC<MapViewProps> = ({
  onViewportChange,
  children,
}) => {
  return (
    <div className="relative w-full h-full">
      <MapContainer
        center={[20, 0]}
        zoom={2}
        className="w-full h-full"
        zoomControl={true}
      >
        <TileLayer
          attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
          url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        />

        <MapEventHandler onViewportChange={onViewportChange} />

        {/* Custom layers passed as children */}
        {children}
      </MapContainer>
    </div>
  );
};
