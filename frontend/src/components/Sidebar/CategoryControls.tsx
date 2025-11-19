/**
 * CategoryControls Component
 *
 * Multi-select controls for filtering by category
 */

import React from 'react';
import { Category } from '../../types/dataset';
import { Card, CardHeader, CardTitle, CardContent } from '../ui/card';
import { Badge } from '../ui/badge';

interface CategoryControlsProps {
  categories: Category[];
  selectedCategories: number[];
  onCategoryToggle: (categoryId: number) => void;
  disabled?: boolean;
}

export const CategoryControls: React.FC<CategoryControlsProps> = ({
  categories,
  selectedCategories,
  onCategoryToggle,
  disabled,
}) => {
  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-lg">Categories</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        {categories.map((category) => {
          const isSelected = selectedCategories.includes(category.id);

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
                type="checkbox"
                checked={isSelected}
                onChange={() => onCategoryToggle(category.id)}
                disabled={disabled}
                className="mt-1 h-4 w-4 rounded border-gray-300"
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

        {selectedCategories.length > 0 && (
          <div className="pt-2 border-t">
            <p className="text-xs text-muted-foreground">
              {selectedCategories.length} of {categories.length} selected
            </p>
          </div>
        )}
      </CardContent>
    </Card>
  );
};
