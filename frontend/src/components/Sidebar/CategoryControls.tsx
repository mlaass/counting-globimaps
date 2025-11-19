/**
 * CategoryControls Component
 *
 * Radio button controls for selecting a single category
 */

import React from 'react';
import { Category } from '../../types/dataset';
import { Card, CardHeader, CardTitle, CardContent } from '../ui/card';
import { Badge } from '../ui/badge';

interface CategoryControlsProps {
  categories: Category[];
  selectedCategory: number | null;
  onCategorySelect: (categoryId: number | null) => void;
  disabled?: boolean;
}

export const CategoryControls: React.FC<CategoryControlsProps> = ({
  categories,
  selectedCategory,
  onCategorySelect,
  disabled,
}) => {
  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-lg">Category</CardTitle>
      </CardHeader>
      <CardContent className="space-y-2">
        {/* All Categories option */}
        <label
          className={`
            flex items-start gap-3 p-3 rounded-lg border cursor-pointer
            transition-colors
            ${selectedCategory === null ? 'bg-accent border-primary' : 'bg-card hover:bg-accent/50'}
            ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
          `}
        >
          <input
            type="radio"
            name="category"
            checked={selectedCategory === null}
            onChange={() => onCategorySelect(null)}
            disabled={disabled}
            className="mt-1 h-4 w-4"
          />
          <div className="flex-1">
            <span className="font-medium text-sm">All Categories</span>
            <p className="text-xs text-muted-foreground mt-1">
              Show all events regardless of category
            </p>
          </div>
        </label>

        {/* Individual category options */}
        {categories.map((category) => {
          const isSelected = selectedCategory === category.id;

          return (
            <label
              key={category.id}
              className={`
                flex items-start gap-3 p-3 rounded-lg border cursor-pointer
                transition-colors
                ${isSelected ? 'bg-accent border-primary' : 'bg-card hover:bg-accent/50'}
                ${disabled ? 'opacity-50 cursor-not-allowed' : ''}
              `}
            >
              <input
                type="radio"
                name="category"
                checked={isSelected}
                onChange={() => onCategorySelect(category.id)}
                disabled={disabled}
                className="mt-1 h-4 w-4"
              />
              <div className="flex-1 min-w-0">
                <div className="flex items-center gap-2">
                  <span className="font-medium text-sm">{category.name}</span>
                  <Badge
                    variant="secondary"
                    className="h-3 w-3 rounded-full p-0"
                    style={{ backgroundColor: category.color }}
                  />
                </div>
                <p className="text-xs text-muted-foreground mt-1">
                  {category.description}
                </p>
              </div>
            </label>
          );
        })}

        {categories.length === 0 && (
          <p className="text-sm text-muted-foreground text-center py-4">
            No categories available
          </p>
        )}
      </CardContent>
    </Card>
  );
};
